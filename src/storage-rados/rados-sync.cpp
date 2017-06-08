/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

extern "C" {
#include "lib.h"
#include "typeof-def.h"
#include "ioloop.h"
#include "str.h"
#include "guid.h"

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

// TODO(jrse) nearly a copy of
//            static int rados_get_index_record(struct mail *_mail)
// in rados-mail.cpp
static int rados_get_index_record(struct mail_index_view *_sync_view, uint32_t seq, uint32_t ext_id,
                                  guid_128_t *index_oid) {
  FUNC_START();

  if (guid_128_is_empty(*index_oid)) {
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
  }

  FUNC_END();
  return 0;
}
static void rados_sync_expunge(struct rados_sync_context *ctx, uint32_t seq1, uint32_t seq2) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  uint32_t uid;
  guid_128_t oid;

  for (; seq1 <= seq2; seq1++) {
    mail_index_lookup_uid(ctx->sync_view, seq1, &uid);
    mail_index_expunge(ctx->trans, seq1);

    struct expunged_item *item = p_new(default_pool, struct expunged_item, 1);
    item->uid = uid;
    if (rados_get_index_record(ctx->sync_view, seq1, ((struct rados_mailbox *)box)->ext_id, &item->oid) < 0) {
      // continue anyway
    } else {
      array_append(&ctx->expunged_items, &item, 1);
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
  i_array_init(&ctx->expunged_items, 32);

  sync_flags = index_storage_get_sync_flags(&mbox->box) | MAIL_INDEX_SYNC_FLAG_FLUSH_DIRTY;
  if (!force)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_REQUIRE_CHANGES;

  ret = index_storage_expunged_sync_begin(&mbox->box, &ctx->index_sync_ctx, &ctx->sync_view, &ctx->trans,
                                          static_cast<mail_index_sync_flags>(sync_flags));
  if (ret <= 0) {
    debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_begin (ret <= 0, 1)", NULL);
    array_free(&ctx->expunged_items);
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

static void rados_sync_object_expunge(struct rados_sync_context *ctx, struct expunged_item *item) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  struct dbox_file *file;
  struct rbox_file *sfile;
  int ret;

  struct rados_storage *r_storage = (struct rados_storage *)box->storage;
  ret = r_storage->s->get_io_ctx().remove(guid_128_to_string(item->oid));

  /* do sync_notify only when the file was unlinked by us */
  if (ret > 0 && box->v.sync_notify != NULL)
    box->v.sync_notify(box, item->uid, MAILBOX_SYNC_TYPE_EXPUNGE);

  FUNC_END();
}
static void rados_sync_expunge_rados_objects(struct rados_sync_context *ctx) {
  FUNC_START();
  struct expunged_item *const *items, *item;
  unsigned int count;
  int i;
  /* NOTE: Index is no longer locked. Multiple processes may be deleting
     the objects at the same time. */
  ctx->mbox->box.tmp_sync_view = ctx->sync_view;

  // rados_sync_object_expunge;
  items = array_get(&ctx->expunged_items, &count);

  for (i = 0; i < count; i++) {
    T_BEGIN {
      item = items[i];
      rados_sync_object_expunge(ctx, item);
    }
    T_END;
  }

  if (ctx->mbox->box.v.sync_notify != NULL)
    ctx->mbox->box.v.sync_notify(&ctx->mbox->box, 0, 0);

  ctx->mbox->box.tmp_sync_view = NULL;
  FUNC_END();
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
    } else {
      rados_sync_expunge_rados_objects(ctx);
    }
  } else {
    mail_index_sync_rollback(&ctx->index_sync_ctx);
  }

  index_storage_expunging_deinit(&ctx->mbox->box);

  array_delete(&ctx->expunged_items, array_count(&ctx->expunged_items) - 1, 1);
  array_free(&ctx->expunged_items);

  debug_print_rados_sync_context(ctx, "rados-sync::rados_sync_finish", NULL);

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
