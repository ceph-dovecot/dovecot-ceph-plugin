// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 * Copyright (c) 2007-2017 Dovecot authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include <rados/librados.hpp>

extern "C" {
#include "dovecot-all.h"
#include "mailbox-recent-flags.h"

#include "rbox-sync.h"
#include "debug-helper.h"
}
#include "rados-util.h"
#include "rbox-storage.hpp"
#include "rbox-mail.h"
#include "rbox-sync-rebuild.h"

#define RBOX_REBUILD_COUNT 3


// TODO(jrse) nearly a copy of
//            static int rbox_get_index_record(struct mail *_mail)
// in rbox-mail.cpp
static int rbox_get_index_record(struct mail_index_view *_sync_view, uint32_t seq, uint32_t ext_id,
                                 guid_128_t *index_oid) {
  FUNC_START();

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

  FUNC_END();
  return 0;
}

static void rbox_sync_expunge(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  uint32_t uid;

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
  FUNC_END();
}

static int update_extended_metadata(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2, const int &keyword_idx,
                                    bool remove) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  uint32_t uid;
  int ret = 0;
  if (rbox_open_rados_connection(box) < 0) {
    i_error("rbox_sync_object_expunge: connection to rados failed");
    return -1;
  }
  for (; seq1 <= seq2; seq1++) {
    mail_index_lookup_uid(ctx->sync_view, seq1, &uid);
    guid_128_t index_oid;
    if (rbox_get_index_record(ctx->sync_view, seq1, ((struct rbox_mailbox *)box)->ext_id, &index_oid) >= 0) {
      const char *oid = guid_128_to_string(index_oid);

      struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
      std::string key_oid(oid);
      std::string key_value = std::to_string(keyword_idx);
      std::string ext_key = "k_" + key_value;
      if (remove) {
        ret = r_storage->s->remove_extended_metadata(key_oid, ext_key);
      } else {
        librmb::RadosMetadata ext_metata(ext_key, key_value);
        ret = r_storage->s->update_extended_metadata(key_oid, &ext_metata);
      }
      if (ret < 0) {
        break;
      }
    }
  }
  FUNC_END();
  return ret;
}
static int update_flags(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2, uint8_t &add_flags,
                        uint8_t &remove_flags) {
  FUNC_START();
  struct mailbox *box = &ctx->mbox->box;
  uint32_t uid;
  int ret = 0;
  if (rbox_open_rados_connection(box) < 0) {
    i_error("rbox_sync_object_expunge: connection to rados failed");
    return -1;
  }
  for (; seq1 <= seq2; seq1++) {
    mail_index_lookup_uid(ctx->sync_view, seq1, &uid);
    guid_128_t index_oid;
    if (rbox_get_index_record(ctx->sync_view, seq1, ((struct rbox_mailbox *)box)->ext_id, &index_oid) >= 0) {
      const char *oid = guid_128_to_string(index_oid);

      struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
      librmb::RadosMailObject mail_object;
      mail_object.set_oid(oid);
      r_storage->s->load_metadata(&mail_object);
      std::string flags_metadata = mail_object.get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS);
      uint8_t flags = librmb::RadosUtils::string_to_flags(flags_metadata);

      if (flags != 0) {
        if (add_flags != 0) {
          flags |= add_flags;
        }
        if (remove_flags != 0) {
          flags &= ~remove_flags;
        }

        flags_metadata = librmb::RadosUtils::flags_to_string(flags);
        librmb::RadosMetadata update(librmb::RBOX_METADATA_OLDV1_FLAGS, flags_metadata);
        ret = r_storage->s->set_metadata(mail_object.get_oid(), update);
      }
    }
  }
  FUNC_END();
  return ret;
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
    mail_storage_set_critical(box->storage, "rbox %s: Broken index: missing UIDVALIDITY", mailbox_get_path(box));
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
    struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

    switch (sync_rec.type) {
      case MAIL_INDEX_SYNC_TYPE_EXPUNGE:
        rbox_sync_expunge(ctx, seq1, seq2);
        break;
      case MAIL_INDEX_SYNC_TYPE_FLAGS:
        if (r_storage->s->get_rados_config()->is_mutable_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS)) {
          update_flags(ctx, seq1, seq2, sync_rec.add_flags, sync_rec.remove_flags);
        }
        break;
      case MAIL_INDEX_SYNC_TYPE_KEYWORD_ADD:
        if (r_storage->s->get_rados_config()->is_mutable_metadata(librmb::RBOX_METADATA_OLDV1_KEYWORDS)) {
          // sync_rec.keyword_idx;
          if (update_extended_metadata(ctx, seq1, seq2, sync_rec.keyword_idx, false) < 0) {
            return -1;
          }
        }
        break;
      case MAIL_INDEX_SYNC_TYPE_KEYWORD_REMOVE:
        if (r_storage->s->get_rados_config()->is_mutable_metadata(librmb::RBOX_METADATA_OLDV1_KEYWORDS)) {
          /* FIXME: should be bother calling sync_notify()? */
          // sync_rec.keyword_idx
          if (update_extended_metadata(ctx, seq1, seq2, sync_rec.keyword_idx, true) < 0) {
            return -1;
          }
        }
        break;
      default:
        break;
    }
  }

  if (box->v.sync_notify != NULL)
    box->v.sync_notify(box, 0, static_cast<mailbox_sync_type>(0));

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

int rbox_sync_begin(struct rbox_mailbox *mbox, struct rbox_sync_context **ctx_r, enum rbox_sync_flags flags) {
  FUNC_START();
  struct rbox_sync_context *ctx;
  int sync_flags;
  int ret;
  struct mail_storage *storage = mbox->box.storage;

  unsigned int i;
  bool rebuild, force_rebuild;

  force_rebuild = (flags & RBOX_SYNC_FLAG_FORCE_REBUILD) != 0;
  rebuild = force_rebuild || mbox->corrupted_rebuild_count != 0 || rbox_refresh_header(mbox, TRUE, FALSE) < 0;
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_MAIL_INDEX_HDR_FLAG_FSCKD
  const struct mail_index_header *hdr = mail_index_get_header(mbox->box.view);
  rebuild = (hdr->flags & MAIL_INDEX_HDR_FLAG_FSCKD) != 0;
#endif

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
    // sync_ctx->flags werden gesetzt.
    ret = index_storage_expunged_sync_begin(&mbox->box, &ctx->index_sync_ctx, &ctx->sync_view, &ctx->trans,
                                            static_cast<mail_index_sync_flags>(sync_flags));

    if (mail_index_reset_fscked(mbox->box.index))
      rbox_set_mailbox_corrupted(&mbox->box);

    if (ret <= 0) {
      array_delete(&ctx->expunged_items, array_count(&ctx->expunged_items) - 1, 1);
      array_free(&ctx->expunged_items);
      i_free(ctx);
      *ctx_r = NULL;
      FUNC_END_RET("ret <= 0");
      return ret;
    }
    if (rebuild) {
      ret = 0;
    } else {
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

  *ctx_r = ctx;
  FUNC_END();
  return 0;
}

static void rbox_sync_object_expunge(struct rbox_sync_context *ctx, struct expunged_item *item) {
  FUNC_START();

  struct mailbox *box = &ctx->mbox->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  const char *oid = guid_128_to_string(item->oid);

  if (rbox_open_rados_connection(box) < 0) {
    i_error("rbox_sync_object_expunge: connection to rados failed");
    return;
  }
  i_debug("deleting mail: %s, in namespace %s", oid, r_storage->s->get_namespace().c_str());
  r_storage->s->delete_mail(oid);
  // callback
  /* do sync_notify only when the file was unlinked by us */
  if (box->v.sync_notify != NULL) {
    i_debug("sync: notify oid: %s", guid_128_to_string(item->oid));
    box->v.sync_notify(box, item->uid, MAILBOX_SYNC_TYPE_EXPUNGE);
  }
  i_debug("sync: expunge object: oid=%s, process-id=%d", guid_128_to_string(item->oid), getpid());

  FUNC_END();
}

static void rbox_sync_expunge_rbox_objects(struct rbox_sync_context *ctx) {
  FUNC_START();
  struct expunged_item *const *items, *item;
  struct expunged_item *const *moved_items, *moved_item;
  unsigned int count, moved_count;
  unsigned int i, j;

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
  struct expunged_item *const *exp_items, *exp_item;
  unsigned int count, i;

  *_ctx = NULL;
  if (success) {
    mail_index_view_ref(ctx->sync_view);
    if (mail_index_sync_commit(&ctx->index_sync_ctx) < 0) {
      mailbox_set_index_error(&ctx->mbox->box);
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

  if (array_is_created(&ctx->expunged_items)) {
    if (array_count(&ctx->expunged_items) > 0) {
      exp_items = array_get(&ctx->expunged_items, &count);
      for (i = 0; i < count; i++) {
        exp_item = exp_items[i];
        i_free(exp_item);
      }
      array_delete(&ctx->expunged_items, array_count(&ctx->expunged_items) - 1, 1);
    }
    array_free(&ctx->expunged_items);
  }

  i_free(ctx);
  FUNC_END();
  return ret;
}

int rbox_sync(struct rbox_mailbox *mbox, enum rbox_sync_flags flags) {
  FUNC_START();
  struct rbox_sync_context *sync_ctx;

  if (rbox_sync_begin(mbox, &sync_ctx, flags) < 0) {
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

    return ctx;
   */
}
