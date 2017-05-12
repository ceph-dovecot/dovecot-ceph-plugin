/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "lib.h"
#include "dbox-attachment.h"
#include "rbox-storage.h"
#include "rbox-file.h"
#include "rbox-sync.h"
#include "debug-helper.h"

#define RBOX_REBUILD_COUNT 3

static void dbox_sync_file_move_if_needed(struct dbox_file *file, enum rbox_sync_entry_type type) {
  FUNC_START();
  struct stat st;
  bool move_to_alt = type == RBOX_SYNC_ENTRY_TYPE_MOVE_TO_ALT;
  bool deleted;

  if (move_to_alt == dbox_file_is_in_alt(file) && !move_to_alt) {
    /* unopened dbox files default to primary dir.
       stat the file to update its location. */
    (void)dbox_file_stat(file, &st);
  }
  if (move_to_alt != dbox_file_is_in_alt(file)) {
    /* move the file. if it fails, nothing broke so
       don't worry about it. */
    if (dbox_file_open(file, &deleted) > 0 && !deleted)
      (void)rbox_file_move(file, move_to_alt);
  }
  rbox_dbg_print_dbox_file(file, "dbox_sync_file_move_if_needed", NULL);
  FUNC_END();
}

static void rbox_sync_file(struct rbox_sync_context *ctx, uint32_t seq, uint32_t uid, enum rbox_sync_entry_type type) {
  FUNC_START();
  struct dbox_file *file;
  enum modify_type modify_type;

  i_debug("rbox_sync_file: seq = %u, uid = %u, type = %d", seq, uid, type);
  rbox_dbg_print_rbox_sync_context(ctx, "rbox_sync_file", NULL);

  switch (type) {
    case RBOX_SYNC_ENTRY_TYPE_EXPUNGE:
      if (!mail_index_transaction_is_expunged(ctx->trans, seq)) {
        mail_index_expunge(ctx->trans, seq);
        array_append(&ctx->expunged_uids, &uid, 1);
      }
      break;
    case RBOX_SYNC_ENTRY_TYPE_MOVE_FROM_ALT:
    case RBOX_SYNC_ENTRY_TYPE_MOVE_TO_ALT:
      /* update flags in the sync transaction, mainly to make
         sure that these alt changes get marked as synced
         and won't be retried */
      modify_type = type == RBOX_SYNC_ENTRY_TYPE_MOVE_TO_ALT ? MODIFY_ADD : MODIFY_REMOVE;
      mail_index_update_flags(ctx->trans, seq, modify_type, (enum mail_flags)DBOX_INDEX_FLAG_ALT);
      file = rbox_file_init(ctx->mbox, uid);
      dbox_sync_file_move_if_needed(file, type);
      dbox_file_unref(&file);
      break;
  }
  FUNC_END();
}

static void rbox_sync_add(struct rbox_sync_context *ctx, const struct mail_index_sync_rec *sync_rec) {
  FUNC_START();
  uint32_t uid;
  enum rbox_sync_entry_type type;
  uint32_t seq, seq1, seq2;

  i_debug("rbox_sync_add: type = %d", sync_rec->type);
  rbox_dbg_print_rbox_sync_context(ctx, "rbox_sync_add", NULL);

  if (sync_rec->type == MAIL_INDEX_SYNC_TYPE_EXPUNGE) {
    /* we're interested */
    type = RBOX_SYNC_ENTRY_TYPE_EXPUNGE;
  } else if (sync_rec->type == MAIL_INDEX_SYNC_TYPE_FLAGS) {
    /* we care only about alt flag changes */
    if ((sync_rec->add_flags & DBOX_INDEX_FLAG_ALT) != 0)
      type = RBOX_SYNC_ENTRY_TYPE_MOVE_TO_ALT;
    else if ((sync_rec->remove_flags & DBOX_INDEX_FLAG_ALT) != 0)
      type = RBOX_SYNC_ENTRY_TYPE_MOVE_FROM_ALT;
    else
      return;
  } else {
    /* not interested */
    return;
  }

  if (!mail_index_lookup_seq_range(ctx->sync_view, sync_rec->uid1, sync_rec->uid2, &seq1, &seq2)) {
    /* already expunged everything. nothing to do. */
    return;
  }

  for (seq = seq1; seq <= seq2; seq++) {
    mail_index_lookup_uid(ctx->sync_view, seq, &uid);
    rbox_sync_file(ctx, seq, uid, type);
  }
  FUNC_END();
}

static int rbox_sync_index(struct rbox_sync_context *ctx) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  const struct mail_index_header *hdr;
  struct mail_index_sync_rec sync_rec;
  uint32_t seq1, seq2;

  hdr = mail_index_get_header(ctx->sync_view);

  i_debug("rbox_sync_index: hdr->uid_validity = %u", hdr->uid_validity);
  rbox_dbg_print_rbox_sync_context(ctx, "rbox_sync_index", NULL);

  if (hdr->uid_validity == 0) {
    /* newly created index file */
    if (hdr->next_uid == 1) {
      /* could be just a race condition where we opened the
         mailbox between mkdir and index creation. fix this
         silently. */
      if (rbox_mailbox_create_indexes(box, NULL, ctx->trans) < 0) {
        FUNC_END_RET("ret == -1");
        return -1;
      }
      FUNC_END_RET("ret == 1");
      return 1;
    }
    mail_storage_set_critical(box->storage, "rbox %s: Broken index: missing UIDVALIDITY", mailbox_get_path(box));
    rbox_set_mailbox_corrupted(box);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  /* mark the newly seen messages as recent */
  if (mail_index_lookup_seq_range(ctx->sync_view, hdr->first_recent_uid, hdr->next_uid, &seq1, &seq2))
    mailbox_recent_flags_set_seqs(box, ctx->sync_view, seq1, seq2);

  while (mail_index_sync_next(ctx->index_sync_ctx, &sync_rec))
    rbox_sync_add(ctx, &sync_rec);
  FUNC_END();
  return 1;
}

static void dbox_sync_file_expunge(struct rbox_sync_context *ctx, uint32_t uid) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  struct dbox_file *file;
  struct rbox_file *sfile;
  int ret;

  i_debug("dbox_sync_file_expunge: uid = %u", uid);
  rbox_dbg_print_rbox_sync_context(ctx, "dbox_sync_file_expunge", NULL);

  file = rbox_file_init(ctx->mbox, uid);
  sfile = (struct rbox_file *)file;
  if (file->storage->attachment_dir != NULL)
    ret = rbox_file_unlink_with_attachments(sfile);
  else
    ret = dbox_file_unlink(file);

  /* do sync_notify only when the file was unlinked by us */
  if (ret > 0 && box->v.sync_notify != NULL)
    box->v.sync_notify(box, uid, MAILBOX_SYNC_TYPE_EXPUNGE);
  dbox_file_unref(&file);
  FUNC_END();
}

static void dbox_sync_expunge_files(struct rbox_sync_context *ctx) {
  FUNC_START();
  const uint32_t *uidp;

  rbox_dbg_print_rbox_sync_context(ctx, "dbox_sync_expunge_files", NULL);

  /* NOTE: Index is no longer locked. Multiple processes may be unlinking
     the files at the same time. */
  ctx->mbox->box.tmp_sync_view = ctx->sync_view;
  array_foreach(&ctx->expunged_uids, uidp) T_BEGIN { dbox_sync_file_expunge(ctx, *uidp); }
  T_END;
  if (ctx->mbox->box.v.sync_notify != NULL)
    ctx->mbox->box.v.sync_notify(&ctx->mbox->box, 0, 0);
  ctx->mbox->box.tmp_sync_view = NULL;
  FUNC_END();
}

static int rbox_refresh_header(struct rbox_mailbox *mbox, bool retry, bool log_error) {
  FUNC_START();
  struct mail_index_view *view;
  struct rbox_index_header hdr;
  bool need_resize;
  int ret;

  view = mail_index_view_open(mbox->box.index);
  ret = rbox_read_header(mbox, &hdr, log_error, &need_resize);
  mail_index_view_close(&view);

  if (ret < 0 && retry) {
    mail_index_refresh(mbox->box.index);
    FUNC_END_RET("retry");
    return rbox_refresh_header(mbox, FALSE, log_error);
  }
  FUNC_END();
  return ret;
}

int rbox_sync_begin(struct rbox_mailbox *mbox, enum rbox_sync_flags flags, struct rbox_sync_context **ctx_r) {
  FUNC_START();
  struct mail_storage *storage = mbox->box.storage;
  const struct mail_index_header *hdr = mail_index_get_header(mbox->box.view);
  struct rbox_sync_context *ctx;
  enum mail_index_sync_flags sync_flags;
  unsigned int i;
  int ret;
  bool rebuild, force_rebuild;

  i_debug("rbox_sync_begin: flags = %d", flags);
  rbox_dbg_print_rbox_mailbox(mbox, "rbox_sync_begin", NULL);

  force_rebuild = (flags & RBOX_SYNC_FLAG_FORCE_REBUILD) != 0;
  rebuild = force_rebuild || (hdr->flags & MAIL_INDEX_HDR_FLAG_FSCKD) != 0 || mbox->corrupted_rebuild_count != 0 ||
            rbox_refresh_header(mbox, TRUE, FALSE) < 0;

  ctx = i_new(struct rbox_sync_context, 1);
  ctx->mbox = mbox;
  ctx->flags = flags;
  i_array_init(&ctx->expunged_uids, 32);

  sync_flags = index_storage_get_sync_flags(&mbox->box);
  if (!rebuild && (flags & RBOX_SYNC_FLAG_FORCE) == 0)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_REQUIRE_CHANGES;
  if ((flags & RBOX_SYNC_FLAG_FSYNC) != 0)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_FSYNC;
  /* don't write unnecessary dirty flag updates */
  sync_flags |= MAIL_INDEX_SYNC_FLAG_AVOID_FLAG_UPDATES;

  for (i = 0;; i++) {
    ret = index_storage_expunged_sync_begin(&mbox->box, &ctx->index_sync_ctx, &ctx->sync_view, &ctx->trans, sync_flags);
    if (mail_index_reset_fscked(mbox->box.index))
      rbox_set_mailbox_corrupted(&mbox->box);
    if (ret <= 0) {
      array_free(&ctx->expunged_uids);
      i_free(ctx);
      *ctx_r = NULL;
      FUNC_END_RET("ret");
      return ret;
    }

    if (rebuild)
      ret = 0;
    else {
      if ((ret = rbox_sync_index(ctx)) > 0)
        break;
    }

    /* failure. keep the index locked while we're doing a
       rebuild. */
    if (ret == 0) {
      if (i >= RBOX_REBUILD_COUNT) {
        mail_storage_set_critical(storage, "rbox %s: Index keeps breaking", mailbox_get_path(&ctx->mbox->box));
        ret = -1;
      } else {
        /* do a full resync and try again. */
        rebuild = FALSE;
        ret = rbox_sync_index_rebuild(mbox, force_rebuild);
      }
    }
    mail_index_sync_rollback(&ctx->index_sync_ctx);
    if (ret < 0) {
      index_storage_expunging_deinit(&ctx->mbox->box);
      array_free(&ctx->expunged_uids);
      i_free(ctx);
      FUNC_END_RET("ret = -1");
      return -1;
    }
  }

  *ctx_r = ctx;
  FUNC_END();
  return 0;
}

int rbox_sync_finish(struct rbox_sync_context **_ctx, bool success) {
  FUNC_START();
  struct rbox_sync_context *ctx = *_ctx;
  int ret = success ? 0 : -1;

  i_debug("rbox_sync_finish: success = %s", btoa(success));
  rbox_dbg_print_rbox_sync_context(ctx, "rbox_sync_finish", NULL);

  *_ctx = NULL;

  if (success) {
    mail_index_view_ref(ctx->sync_view);

    if (mail_index_sync_commit(&ctx->index_sync_ctx) < 0) {
      mailbox_set_index_error(&ctx->mbox->box);
      ret = -1;
    } else {
      dbox_sync_expunge_files(ctx);
      mail_index_view_close(&ctx->sync_view);
    }
  } else {
    mail_index_sync_rollback(&ctx->index_sync_ctx);
  }

  index_storage_expunging_deinit(&ctx->mbox->box);
  array_free(&ctx->expunged_uids);
  i_free(ctx);
  FUNC_END();
  return ret;
}

int rbox_sync(struct rbox_mailbox *mbox, enum rbox_sync_flags flags) {
  FUNC_START();
  struct rbox_sync_context *sync_ctx;

  if (rbox_sync_begin(mbox, flags, &sync_ctx) < 0) {
    FUNC_END_RET("ret == -1; rbox_sync_begin failed");
    return -1;
  }

  if (sync_ctx == NULL) {
    FUNC_END_RET("ret == 0; sync_ctx == NULL");
    return 0;
  }
  int ret = rbox_sync_finish(&sync_ctx, TRUE);
  FUNC_END();
  return ret;
}

struct mailbox_sync_context *rbox_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  enum rbox_sync_flags rbox_sync_flags = 0;
  int ret = 0;

  i_debug("rbox_sync_begin: flags = %d", flags);
  rbox_dbg_print_rbox_mailbox(mbox, "rbox_sync_begin", NULL);

  if (!box->opened) {
    if (mailbox_open(box) < 0)
      ret = -1;
  }

  if (ret == 0 && mail_index_reset_fscked(box->index))
    rbox_set_mailbox_corrupted(box);
  if (ret == 0 && (index_mailbox_want_full_sync(&mbox->box, flags) || mbox->corrupted_rebuild_count != 0)) {
    if ((flags & MAILBOX_SYNC_FLAG_FORCE_RESYNC) != 0)
      rbox_sync_flags |= RBOX_SYNC_FLAG_FORCE_REBUILD;
    ret = rbox_sync(mbox, rbox_sync_flags);
  }

  FUNC_END();
  return index_mailbox_sync_init(box, flags, ret < 0);
}
