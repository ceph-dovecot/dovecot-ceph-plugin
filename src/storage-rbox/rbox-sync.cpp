
/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

extern "C" {
#include "lib.h"
#include "typeof-def.h"
#include "ioloop.h"
#include "str.h"
#include "guid.h"
#include "dirent.h"
#include "rbox-sync.h"
#include "debug-helper.h"
#include "index-rebuild.h"
#include "string.h"
}
#include <rados/librados.hpp>
#include "rbox-storage.hpp"
#include "rbox-mail.h"
#include "rbox-sync-rebuild.h"
#define RBOX_REBUILD_COUNT 3
using namespace librados;

static void rbox_sync_set_uidvalidity(struct rbox_sync_context *ctx) {
  FUNC_START();
  uint32_t uid_validity = ioloop_time;

  mail_index_update_header(ctx->trans, offsetof(struct mail_index_header, uid_validity), &uid_validity,
                           sizeof(uid_validity), TRUE);
  ctx->uid_validity = uid_validity;
  debug_print_rbox_sync_context(ctx, "rbox-sync::rbox_sync_set_uidvalidity", NULL);
  FUNC_END();
}

// TODO(jrse) nearly a copy of
//            static int rbox_get_index_record(struct mail *_mail)
// in rbox-mail.cpp
static int rbox_get_index_record(struct mail_index_view *_sync_view, uint32_t seq, uint32_t ext_id,
                                 guid_128_t *index_oid) {
  FUNC_START();

  // if (guid_128_is_empty(*index_oid)) {
  const struct obox_mail_index_record *obox_rec;
  const void *rec_data;
  mail_index_lookup_ext(_sync_view, seq, ext_id, &rec_data, NULL);
  obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);

  if (obox_rec == nullptr) {
    /* lost for some reason, give up */
    FUNC_END_RET("ret == -1");
    return -1;
  }
  memcpy(index_oid, obox_rec->oid, sizeof(obox_rec->oid));
  //}

  FUNC_END();
  return 0;
}

static void rbox_sync_expunge(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  uint32_t uid;
  guid_128_t oid;

  for (; seq1 <= seq2; seq1++) {
    mail_index_lookup_uid(ctx->sync_view, seq1, &uid);
    if (!mail_index_transaction_is_expunged(ctx->trans, seq1)) {
      mail_index_expunge(ctx->trans, seq1);

      struct expunged_item *item = p_new(default_pool, struct expunged_item, 1);
      item->uid = uid;
      if (rbox_get_index_record(ctx->sync_view, seq1, ((struct rbox_mailbox *)box)->ext_id, &item->oid) < 0) {
        // continue anyway
      } else {
        array_append(&ctx->expunged_items, &item, 1);
      }
    }
  }
  debug_print_rbox_sync_context(ctx, "rbox-sync::rbox_sync_expunge", NULL);
  FUNC_END();
}

static int rbox_sync_index(struct rbox_sync_context *ctx) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  const struct mail_index_header *hdr;
  struct mail_index_sync_rec sync_rec;
  uint32_t seq1, seq2;

  hdr = mail_index_get_header(ctx->sync_view);
  if (hdr->uid_validity == 0) {
    /* newly created index file */
    if (hdr->next_uid == 1) {
      /* could be just a race condition where we opened the
         mailbox between mkdir and index creation. fix this
         silently. */
      if (rbox_mailbox_create_indexes(box, NULL, ctx->trans) < 0)
        return -1;
      return 1;
    }
    mail_storage_set_critical(box->storage, "sdbox %s: Broken index: missing UIDVALIDITY", mailbox_get_path(box));
    rbox_set_mailbox_corrupted(box);
    return 0;
  }
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
        rbox_sync_expunge(ctx, seq1, seq2);
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

  debug_print_rbox_sync_context(ctx, "rbox-sync::rbox_sync_index", NULL);

  FUNC_END();
  return 1;
}

static int rbox_refresh_header(struct rbox_mailbox *mbox, bool retry, bool log_error) {
  struct mail_index_view *view;
  struct sdbox_index_header hdr;
  bool need_resize;
  int ret;

  view = mail_index_view_open(mbox->box.index);
  ret = rbox_read_header(mbox, &hdr, log_error, &need_resize);
  mail_index_view_close(&view);

  if (ret < 0 && retry) {
    mail_index_refresh(mbox->box.index);
    return rbox_refresh_header(mbox, FALSE, log_error);
  }
  return ret;
}

int rbox_sync_begin(struct rbox_mailbox *mbox, struct rbox_sync_context **ctx_r, bool force,
                    enum rbox_sync_flags flags) {
  FUNC_START();
  struct rbox_sync_context *ctx;
  int sync_flags;
  int ret;
  struct mail_storage *storage = mbox->box.storage;

  unsigned int i;
  bool rebuild, force_rebuild;
  const struct mail_index_header *hdr = mail_index_get_header(mbox->box.view);

  force_rebuild = (flags & RBOX_SYNC_FLAG_FORCE_REBUILD) != 0;
  rebuild = force_rebuild || (hdr->flags & MAIL_INDEX_HDR_FLAG_FSCKD) != 0 || mbox->corrupted_rebuild_count != 0 ||
            rbox_refresh_header(mbox, TRUE, FALSE) < 0;

  ctx = i_new(struct rbox_sync_context, 1);
  ctx->mbox = mbox;
  i_array_init(&ctx->expunged_items, 32);

  sync_flags = index_storage_get_sync_flags(&mbox->box);
  if (!rebuild && (flags & RBOX_SYNC_FLAG_FORCE) == 0)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_REQUIRE_CHANGES;
  if ((flags & RBOX_SYNC_FLAG_FSYNC) != 0)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_FSYNC;
  /* don't write unnecessary dirty flag updates */
  sync_flags |= MAIL_INDEX_SYNC_FLAG_AVOID_FLAG_UPDATES;

  for (i = 0;; i++) {
    ret = index_storage_expunged_sync_begin(&mbox->box, &ctx->index_sync_ctx, &ctx->sync_view, &ctx->trans,
                                            static_cast<mail_index_sync_flags>(sync_flags));

    if (mail_index_reset_fscked(mbox->box.index))
      rbox_set_mailbox_corrupted(&mbox->box);

    if (ret <= 0) {
      debug_print_rbox_sync_context(ctx, "rbox-sync::rbox_sync_begin (ret <= 0, 1)", NULL);
      array_delete(&ctx->expunged_items, array_count(&ctx->expunged_items) - 1, 1);
      array_free(&ctx->expunged_items);
      i_free(ctx);
      *ctx_r = NULL;
      FUNC_END_RET("ret <= 0");
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
        mail_storage_set_critical(storage, "bbox %s: Index keeps breaking", mailbox_get_path(&ctx->mbox->box));
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
      array_delete(&ctx->expunged_items, array_count(&ctx->expunged_items) - 1, 1);
      array_free(&ctx->expunged_items);
      i_free(ctx);
      return -1;
    }
  }

  debug_print_rbox_sync_context(ctx, "rbox-sync::rbox_sync_begin", NULL);
  *ctx_r = ctx;
  FUNC_END();
  return 0;
}

static void remove_callback(rados_completion_t comp, void *arg) {
  struct expunge_callback_data *data = (struct expunge_callback_data *)arg;
  // callback
  /* do sync_notify only when the file was unlinked by us */
  if (data->box->v.sync_notify != NULL) {
    i_debug("sync: notify oid: %s", guid_128_to_string(data->item->oid));
    data->box->v.sync_notify(data->box, data->item->uid, MAILBOX_SYNC_TYPE_EXPUNGE);
  }
  i_debug("sync: expunge object: oid=%s, process-id=%d", guid_128_to_string(data->item->oid), getpid());
}

void rbox_sync_object_expunge(struct rbox_sync_context *ctx, struct expunged_item *item) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  int ret;
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  librados::AioCompletion *completion = librados::Rados::aio_create_completion();

  const char *oid = guid_128_to_string(item->oid);

  struct expunge_callback_data *cb_data = i_new(struct expunge_callback_data, 1);
  cb_data->item = item;
  cb_data->box = box;

  completion->set_complete_callback((void *)cb_data, remove_callback);

  if (rbox_open_rados_connection(box) < 0) {
    i_debug("rbox_sync_object_expunge: connection to rados failed");
    return;
  }

  ret = r_storage->s->get_io_ctx().aio_remove(oid, completion);

  completion->wait_for_complete_and_cb();

  FUNC_END();
}

static void rbox_sync_expunge_rbox_objects(struct rbox_sync_context *ctx) {
  FUNC_START();
  struct expunged_item *const *items, *item;
  struct expunged_item *const *moved_items, *moved_item;
  unsigned int count, moved_count;
  int i, j;

  /* NOTE: Index is no longer locked. Multiple processes may be deleting
     the objects at the same time. */
  ctx->mbox->box.tmp_sync_view = ctx->sync_view;

  // rbox_sync_object_expunge;
  items = array_get(&ctx->expunged_items, &count);

  if (count > 0) {
    moved_items = array_get(&ctx->mbox->moved_items, &moved_count);
    for (i = 0; i < count; i++) {
      T_BEGIN {
        item = items[i];
        bool moved = FALSE;
        for (j = 0; j < moved_count; j++) {
          moved_item = moved_items[j];
          if (guid_128_equals(moved_item->oid, item->oid)) {
            moved = TRUE;
            break;
          }
        }
        if (moved != TRUE) {
          rbox_sync_object_expunge(ctx, item);
        }
      }
      T_END;
    }
  }

  if (ctx->mbox->box.v.sync_notify != NULL)
    ctx->mbox->box.v.sync_notify(&ctx->mbox->box, 0, static_cast<mailbox_sync_type>(0));

  ctx->mbox->box.tmp_sync_view = NULL;
  FUNC_END();
}

int rbox_sync_finish(struct rbox_sync_context **_ctx, bool success) {
  FUNC_START();
  struct rbox_sync_context *ctx = *_ctx;
  int ret = success ? 0 : -1;

  *_ctx = NULL;
  if (success) {
    mail_index_view_ref(ctx->sync_view);
    if (mail_index_sync_commit(&ctx->index_sync_ctx) < 0) {
      mailbox_set_index_error(&ctx->mbox->box);
      debug_print_rbox_sync_context(ctx, "rbox-sync::rbox_sync_finish (ret -1, 1)", NULL);
      FUNC_END_RET("ret == -1");
      ret = -1;
    } else {
      rbox_sync_expunge_rbox_objects(ctx);
      mail_index_view_close(&ctx->sync_view);
    }
  } else {
    mail_index_sync_rollback(&ctx->index_sync_ctx);
  }

  index_storage_expunging_deinit(&ctx->mbox->box);

  array_delete(&ctx->expunged_items, array_count(&ctx->expunged_items) - 1, 1);
  array_free(&ctx->expunged_items);

  debug_print_rbox_sync_context(ctx, "rbox-sync::rbox_sync_finish", NULL);

  i_free(ctx);
  FUNC_END();
  return ret;
}

int rbox_sync(struct rbox_mailbox *mbox, enum rbox_sync_flags flags) {
  FUNC_START();
  struct rbox_sync_context *sync_ctx;

  if (rbox_sync_begin(mbox, &sync_ctx, FALSE, flags) < 0) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  FUNC_END();
  return sync_ctx == NULL ? 0 : rbox_sync_finish(&sync_ctx, TRUE);
}

struct mailbox_sync_context *rbox_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  int sdbox_sync_flags = 0;

  int ret = 0;

  if (!box->opened) {
    if (mailbox_open(box) < 0) {
      debug_print_mailbox(box, "rbox-sync::rbox_storage_sync_init (ret -1, 1)", NULL);
      ret = -1;
    }
  }

  if (ret == 0 && mail_index_reset_fscked(box->index))
    rbox_set_mailbox_corrupted(box);
  if (ret == 0 && (index_mailbox_want_full_sync(&mbox->box, flags) || mbox->corrupted_rebuild_count != 0)) {
    if ((flags & MAILBOX_SYNC_FLAG_FORCE_RESYNC) != 0)
      sdbox_sync_flags |= RBOX_SYNC_FLAG_FORCE_REBUILD;
    ret = rbox_sync(mbox, static_cast<rbox_sync_flags>(sdbox_sync_flags));
  }
  FUNC_END();
  return index_mailbox_sync_init(box, flags, ret < 0);
  /*
    if (index_mailbox_want_full_sync(&mbox->box, flags) && ret == 0)
      ret = rbox_sync(mbox);

    struct mailbox_sync_context *ctx = index_mailbox_sync_init(box, flags, ret < 0);

    debug_print_mailbox(box, "rbox-sync::rbox_storage_sync_init", NULL);

    return ctx;*/
}
