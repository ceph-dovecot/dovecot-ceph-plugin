/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

extern "C" {
#include "lib.h"
#include "typeof-def.h"
#include "ioloop.h"
#include "str.h"

#include "rados-storage-local.h"
#include "rados-sync.h"
#include "debug-helper.h"
}

#include "rados-storage-struct.h"

static void rados_sync_set_uidvalidity(struct rados_sync_context *ctx) {
  FUNC_START();
  uint32_t uid_validity = ioloop_time;

  mail_index_update_header(ctx->trans, offsetof(struct mail_index_header, uid_validity), &uid_validity,
                           sizeof(uid_validity), TRUE);
  ctx->uid_validity = uid_validity;
  debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_set_uidvalidity", NULL);
  FUNC_END();
}

static string_t *rados_get_path_prefix(struct rados_mailbox *mbox) {
  FUNC_START();
  string_t *path = str_new(default_pool, 256);

  str_append(path, mailbox_get_path(&mbox->box));
  str_append_c(path, '/');
  i_debug("path = %s", (char *)path->data);
  debug_print_mailbox(&mbox->box, "rados-sync::rados_get_path_prefix", NULL);
  FUNC_END();
  return path;
}

static void rados_sync_expunge(struct rados_sync_context *ctx, uint32_t seq1, uint32_t seq2) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  uint32_t uid;

  if (ctx->path == NULL) {
    ctx->path = rados_get_path_prefix(ctx->mbox);
    ctx->path_dir_prefix_len = str_len(ctx->path);
  }

  for (; seq1 <= seq2; seq1++) {
    mail_index_lookup_uid(ctx->sync_view, seq1, &uid);

    str_truncate(ctx->path, ctx->path_dir_prefix_len);
    str_printfa(ctx->path, "%u.", uid);
    if (i_unlink_if_exists(str_c(ctx->path)) < 0) {
      /* continue anyway */
    } else {
      if (box->v.sync_notify != NULL) {
        box->v.sync_notify(box, uid, MAILBOX_SYNC_TYPE_EXPUNGE);
      }
      mail_index_expunge(ctx->trans, seq1);
    }
  }
  debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_expunge", NULL);
  FUNC_END();
}

static void rados_sync_index(struct rados_sync_context *ctx) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  const struct mail_index_header *hdr;
  struct mail_index_sync_rec sync_rec;
  uint32_t seq1, seq2;

  hdr = mail_index_get_header(ctx->sync_view);
  if (hdr->uid_validity != 0)
    ctx->uid_validity = hdr->uid_validity;
  else
    rados_sync_set_uidvalidity(ctx);

  /* mark the newly seen messages as recent */
  if (mail_index_lookup_seq_range(ctx->sync_view, hdr->first_recent_uid, hdr->next_uid, &seq1, &seq2)) {
    mailbox_recent_flags_set_seqs(&ctx->mbox->box, ctx->sync_view, seq1, seq2);
  }

  while (mail_index_sync_next(ctx->index_sync_ctx, &sync_rec)) {
    if (!mail_index_lookup_seq_range(ctx->sync_view, sync_rec.uid1, sync_rec.uid2, &seq1, &seq2)) {
      /* already expunged, nothing to do. */
      continue;
    }

    switch (sync_rec.type) {
      case MAIL_INDEX_SYNC_TYPE_EXPUNGE:
        rados_sync_expunge(ctx, seq1, seq2);
        break;
      case MAIL_INDEX_SYNC_TYPE_FLAGS:
      case MAIL_INDEX_SYNC_TYPE_KEYWORD_ADD:
      case MAIL_INDEX_SYNC_TYPE_KEYWORD_REMOVE:
        /* FIXME: should be bother calling sync_notify()? */
        break;
    }
  }

  if (box->v.sync_notify != NULL)
    box->v.sync_notify(box, 0, static_cast<mailbox_sync_type>(0));

  debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_index", NULL);
  FUNC_END();
}

int rados_sync_begin(struct rados_mailbox *mbox, struct rados_sync_context **ctx_r, bool force) {
  FUNC_START();
  struct rados_sync_context *ctx;
  int sync_flags;
  int ret;

  ctx = i_new(struct rados_sync_context, 1);
  ctx->mbox = mbox;

  sync_flags = index_storage_get_sync_flags(&mbox->box) | MAIL_INDEX_SYNC_FLAG_FLUSH_DIRTY;
  if (!force)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_REQUIRE_CHANGES;

  ret = index_storage_expunged_sync_begin(&mbox->box, &ctx->index_sync_ctx, &ctx->sync_view, &ctx->trans,
                                          static_cast<mail_index_sync_flags>(sync_flags));
  if (ret <= 0) {
    debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_begin (ret <= 0, 1)", NULL);
    i_free(ctx);
    *ctx_r = NULL;
    FUNC_END_RET("ret <= 0");
    return ret;
  }

  rados_sync_index(ctx);
  index_storage_expunging_deinit(&mbox->box);

  debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_begin", NULL);
  *ctx_r = ctx;
  FUNC_END();
  return 0;
}

int rados_sync_finish(struct rados_sync_context **_ctx, bool success) {
  FUNC_START();
  struct rados_sync_context *ctx = *_ctx;
  int ret = success ? 0 : -1;

  *_ctx = NULL;
  if (success) {
    if (mail_index_sync_commit(&ctx->index_sync_ctx) < 0) {
      mailbox_set_index_error(&ctx->mbox->box);
      debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_finish (ret -1, 1)", NULL);
      FUNC_END_RET("ret == -1");
      ret = -1;
    }
  } else {
    mail_index_sync_rollback(&ctx->index_sync_ctx);
  }
  debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_finish", NULL);
  if (ctx->path != NULL)
    str_free(&ctx->path);
  i_free(ctx);
  FUNC_END();
  return ret;
}

int rados_sync(struct rados_mailbox *mbox) {
  FUNC_START();
  struct rados_sync_context *sync_ctx;

  if (rados_sync_begin(mbox, &sync_ctx, FALSE) < 0) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  FUNC_END();
  return sync_ctx == NULL ? 0 : rados_sync_finish(&sync_ctx, TRUE);
}

struct mailbox_sync_context *rados_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags) {
  FUNC_START();
  struct rados_mailbox *mbox = (struct rados_mailbox *)box;
  int ret = 0;

  if (!box->opened) {
    if (mailbox_open(box) < 0) {
      debug_print_mailbox(box, "rados-sync::rados_storage_sync_init (ret -1, 1)", NULL);
      ret = -1;
    }
  }

  if (index_mailbox_want_full_sync(&mbox->box, flags) && ret == 0)
    ret = rados_sync(mbox);

  struct mailbox_sync_context *ctx = index_mailbox_sync_init(box, flags, ret < 0);
  debug_print_mailbox(box, "rados-sync::rados_storage_sync_init", NULL);
  FUNC_END();
  return ctx;
}
