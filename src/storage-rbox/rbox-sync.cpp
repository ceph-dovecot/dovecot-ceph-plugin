// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 * Copyright (c) 2007-2017 Dovecot authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
#include <string>
#include <rados/librados.hpp>
#include <list>
#include <unistd.h>

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

static int rbox_get_oid_from_index(struct mail_index_view *_sync_view, uint32_t seq, uint32_t ext_id,
                                   guid_128_t *index_oid) {
  FUNC_START();

  const struct obox_mail_index_record *obox_rec;
  const void *rec_data;
  mail_index_lookup_ext(_sync_view, seq, ext_id, &rec_data, NULL);
  if (rec_data == NULL) {
    i_error("no index ext entry for %d, ext_id=%d ", seq, ext_id);
    FUNC_END_RET("ret == -1");
    return -1;
  }
  obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);
  memcpy(index_oid, obox_rec->oid, sizeof(obox_rec->oid));
  FUNC_END();
  return 0;
}

static void rbox_sync_expunge(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2) {
  FUNC_START();
  struct mailbox *box = &ctx->rbox->box;
  uint32_t uid = -1;
  uint32_t seq;

  for (seq = seq1; seq <= seq2; seq++) {
    mail_index_lookup_uid(ctx->sync_view, seq, &uid);
    const struct mail_index_record *rec;
    rec = mail_index_lookup(ctx->sync_view, seq);
    if (rec == NULL) {
      i_error("rbox_sync_expunge: mail_index_lookup failed! for %d uid(%d)", seq1, uid);
      continue;  // skip further processing.
    }
    if (!mail_index_transaction_is_expunged(ctx->trans, seq)) {
      /* todo load flags and set alt_storage flag */
      mail_index_expunge(ctx->trans, seq);

      struct expunged_item *item = p_new(default_pool, struct expunged_item, 1);
      i_zero(item);
      item->uid = uid;
      if (rbox_get_oid_from_index(ctx->sync_view, seq, ((struct rbox_mailbox *)box)->ext_id, &item->oid) < 0) {
        // continue anyway
      } else {
        item->alt_storage = is_alternate_storage_set(rec->flags) && is_alternate_pool_valid(box);
        array_append(&ctx->expunged_items, &item, 1);
      }
        
    }
  }
  FUNC_END();
}

static int update_extended_metadata(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2, const int &keyword_idx,
                                    bool remove) {
  FUNC_START();
  uint32_t uid = -1;
  int ret = 0;
  struct mailbox *box = &ctx->rbox->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  for (; seq1 <= seq2; seq1++) {
    mail_index_lookup_uid(ctx->sync_view, seq1, &uid);

    const struct mail_index_record *rec;
    rec = mail_index_lookup(ctx->sync_view, seq1);
    if (rec == NULL) {
      i_error("update_extended_metadata: mail_index_lookup failed! for %d, uid(%d)", seq1, uid);
      continue;  // skip further processing.
    }
    bool alt_storage = is_alternate_storage_set(rec->flags) && is_alternate_pool_valid(box);

    if (rbox_open_rados_connection(box, alt_storage) < 0) {
      i_error("rbox_sync_object_expunge: connection to rados failed. alt_storage(%d)", alt_storage);
      return -1;
    }
    
    r_storage->ms->get_storage()->set_io_ctx(alt_storage ? 
                                             &r_storage->alt->get_io_ctx() : 
                                             &r_storage->s->get_io_ctx() );
    
    guid_128_t index_oid;
    if (rbox_get_oid_from_index(ctx->sync_view, seq1, ((struct rbox_mailbox *)box)->ext_id, &index_oid) >= 0) {
      const char *oid = guid_128_to_string(index_oid);

      std::string key_oid(oid);
      std::string ext_key = std::to_string(keyword_idx);
      if (remove) {
        ret = r_storage->ms->get_storage()->remove_keyword_metadata(key_oid, ext_key);
      } 
      else {
        unsigned int count;
        const char *const *keywords = array_get(&ctx->sync_view->index->keywords, &count);
        if (keywords == NULL) {
          i_error("update_extended_metadata: keywords == NULL , oid(%s), keyword_index(%s)", oid, ext_key.c_str());
          continue;
        }
        std::string key_value = keywords[keyword_idx];
        librmb::RadosMetadata ext_metata(ext_key, key_value);
        ret = r_storage->ms->get_storage()->update_keyword_metadata(key_oid, &ext_metata);
      }
      if (ret < 0) {
        break;
      }
    }
  }
  // reset metadatas storage
  r_storage->ms->get_storage()->set_io_ctx(&r_storage->s->get_io_ctx());
  FUNC_END();
  return ret;
}

static int move_to_alt(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2, bool inverse) {
  struct mailbox *box = &ctx->rbox->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
  int ret = -1;
  // make sure alternative storage is open
  if (rbox_open_rados_connection(box, true) < 0) {
    i_error("move_to_alt: connection to rados failed");
    return -1;
  }
  for (; seq1 <= seq2; seq1++) {
    guid_128_t index_oid;
    if (rbox_get_oid_from_index(ctx->sync_view, seq1, ((struct rbox_mailbox *)&ctx->rbox->box)->ext_id, &index_oid) >= 0) {
      std::string oid = guid_128_to_string(index_oid);
      ret = librmb::RadosUtils::move_to_alt(oid, r_storage->s, r_storage->alt, r_storage->ms, inverse);
      if (ret >= 0) {
        if (inverse) {
          mail_index_update_flags(ctx->trans, seq1, MODIFY_REMOVE, (enum mail_flags)RBOX_INDEX_FLAG_ALT);
        } else {
          mail_index_update_flags(ctx->trans, seq1, MODIFY_ADD, (enum mail_flags)RBOX_INDEX_FLAG_ALT);
        }
      }
    }
  }
  return ret;
}

static int update_flags(struct rbox_sync_context *ctx, uint32_t seq1, uint32_t seq2, uint8_t &add_flags,
                        uint8_t &remove_flags) {
  FUNC_START();
  struct mailbox *box = &ctx->rbox->box;
  uint32_t uid = 0;
  int ret = 0;
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
  bool alt_storage = false;

  for (; seq1 <= seq2; seq1++) {
    mail_index_lookup_uid(ctx->sync_view, seq1, &uid);

    const struct mail_index_record *rec;
    rec = mail_index_lookup(ctx->sync_view, seq1);
    if (rec == NULL) {
      i_error("update_flags: mail_index_lookup failed! for %d, uid(%d)", seq1, uid);
      continue;  // skip further processing.
    }
    alt_storage = is_alternate_storage_set(rec->flags) && is_alternate_pool_valid(box);
    if (rbox_open_rados_connection(box, alt_storage) < 0) {
      i_error("update_flags: connection to rados failed (alt_storage(%d))", alt_storage);
      return -1;
    }
    
    r_storage->ms->get_storage()->set_io_ctx( alt_storage ? 
                                              &r_storage->alt->get_io_ctx() : 
                                              &r_storage->s->get_io_ctx() );
    
    guid_128_t index_oid;
    if (rbox_get_oid_from_index(ctx->sync_view, seq1, ((struct rbox_mailbox *)box)->ext_id, &index_oid) >= 0) {
      const char *oid = guid_128_to_string(index_oid);

      librmb::RadosMail mail_object;
      mail_object.set_oid(oid);
      if (r_storage->ms->get_storage()->load_metadata(&mail_object) < 0) {
        i_error("update_flags: load_metadata failed! for %d, oid(%s)", seq1, oid);
        continue;
      }
      char *flags_metadata = NULL;
      librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS, mail_object.get_metadata(), &flags_metadata);
      uint8_t flags = 0x0;
      if (librmb::RadosUtils::string_to_flags(flags_metadata, &flags)) {
        if (add_flags != 0) {
          flags |= add_flags;
        }
        if (remove_flags != 0) {
          flags &= ~remove_flags;
        }
        std::string str_flags_metadata;
        if (librmb::RadosUtils::flags_to_string(flags, &str_flags_metadata)) {
          librmb::RadosMetadata update(librmb::RBOX_METADATA_OLDV1_FLAGS, str_flags_metadata);
          ret = r_storage->ms->get_storage()->set_metadata(&mail_object, update);
          if (ret < 0) {
            i_warning("updating metadata for object : oid(%s), seq (%d) failed with ceph errorcode: %d",
                      mail_object.get_oid()->c_str(), seq1, ret);
          }
        }
      }
    }
  }
  // reset metadata storage
  r_storage->ms->get_storage()->set_io_ctx(&r_storage->s->get_io_ctx());
  FUNC_END();
  return ret;
}
static int rbox_sync_index(struct rbox_sync_context *ctx) {
  FUNC_START();
  struct mailbox *box = &ctx->rbox->box;
  const struct mail_index_header *hdr;
  struct mail_index_sync_rec sync_rec;
  uint32_t seq1, seq2;
  hdr = mail_index_get_header(ctx->sync_view);
  if (hdr == NULL) {
    i_error("rbox_sync_index: mail_index_get_header failed! ");
    return -1;
  }
  if (hdr->uid_validity == 0) {
    if (hdr->next_uid == 1) {
      /* could be just a race condition where we opened the
         mailbox between mkdir and index creation. fix this
         silently. */
      if (rbox_mailbox_create_indexes(box, NULL, ctx->trans) < 0) {
        return -1;
      }
      return 1;
    }
    mail_storage_set_critical(box->storage, "rbox %s: Broken index: missing UIDVALIDITY", mailbox_get_path(box));
    rbox_set_mailbox_corrupted(box);
    return 0;
  }
  /* mark the newly seen messages as recent */
  if (mail_index_lookup_seq_range(ctx->sync_view, hdr->first_recent_uid, hdr->next_uid, &seq1, &seq2)) {
    mailbox_recent_flags_set_seqs(&ctx->rbox->box, ctx->sync_view, seq1, seq2);
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
        if (is_alternate_storage_set(sync_rec.add_flags) && is_alternate_pool_valid(box)) {
          // move object from mail_storage to apternative_storage.
          int ret = move_to_alt(ctx, seq1, seq2, false);
          if (ret < 0) {
            i_error("Error moving  seq (%d) from alt storage", seq1);
          }
        } 
        else if (is_alternate_storage_set(sync_rec.remove_flags) && is_alternate_pool_valid(box)) {
          // type = SDBOX_SYNC_ENTRY_TYPE_MOVE_FROM_ALT;
          int ret = move_to_alt(ctx, seq1, seq2, true);
          if (ret < 0) {
            i_error("Error moving seq (%d) to alt storage", seq1);
          }
        } else if (r_storage->config->is_mail_attribute(librmb::RBOX_METADATA_OLDV1_FLAGS) &&
                   r_storage->config->is_update_attributes() &&
                   r_storage->config->is_updateable_attribute(librmb::RBOX_METADATA_OLDV1_FLAGS)) {
          if (update_flags(ctx, seq1, seq2, sync_rec.add_flags, sync_rec.remove_flags) < 0) {
            i_error("Error updating flags seq (%d)", seq1);
          }
        }
     
        break;
      case MAIL_INDEX_SYNC_TYPE_KEYWORD_ADD:
        if (r_storage->config->is_mail_attribute(librmb::RBOX_METADATA_OLDV1_KEYWORDS) &&
            r_storage->config->is_update_attributes() &&
            r_storage->config->is_updateable_attribute(librmb::RBOX_METADATA_OLDV1_KEYWORDS)) {
          // sync_rec.keyword_idx;
          if (update_extended_metadata(ctx, seq1, seq2, sync_rec.keyword_idx, false) < 0) {
            return -1;
          }
        }
        break;
      case MAIL_INDEX_SYNC_TYPE_KEYWORD_REMOVE:
        if (r_storage->config->is_mail_attribute(librmb::RBOX_METADATA_OLDV1_KEYWORDS) &&
            r_storage->config->is_update_attributes() &&
            r_storage->config->is_updateable_attribute(librmb::RBOX_METADATA_OLDV1_KEYWORDS)) {
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

  FUNC_END();
  return 1;
}

static int rbox_refresh_header(struct rbox_mailbox *rbox, bool retry, bool log_error) {
  FUNC_START();
  struct mail_index_view *view;
  struct rbox_index_header hdr;
  bool need_resize = false;
  int ret = 0;

  view = mail_index_view_open(rbox->box.index);
  if (view == NULL) {
    i_error("error mail_index_view_open ");
    FUNC_END();
    return -1;
  }
  ret = rbox_read_header(rbox, &hdr, log_error, &need_resize);
  mail_index_view_close(&view);

  if (ret < 0 && retry) {
    mail_index_refresh(rbox->box.index);
    ret = rbox_refresh_header(rbox, FALSE, log_error);
  }
  FUNC_END();
  return ret;
}

int rbox_sync_begin(struct rbox_mailbox *rbox, struct rbox_sync_context **ctx_r, enum rbox_sync_flags flags) {
  FUNC_START();
  struct rbox_sync_context *ctx = NULL;
  struct mail_storage *storage = rbox->box.storage;

  // unsigned int i = 0;
  bool rebuild, force_rebuild;

  force_rebuild = (flags & RBOX_SYNC_FLAG_FORCE_REBUILD) != 0;
  rebuild = force_rebuild || rbox->storage->corrupted_rebuild_count != 0 || rbox_refresh_header(rbox, TRUE, FALSE) < 0;
#ifdef DEBUG
  i_debug("RBOX storage corrupted rebuild count = %d, rebuild %d, force_rebuild %d",
          rbox->storage->corrupted_rebuild_count, rebuild, force_rebuild);
#endif
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_MAIL_INDEX_HDR_FLAG_FSCKD
  const struct mail_index_header *hdr = mail_index_get_header(rbox->box.view);
  // cppcheck-suppress redundantAssignment
  rebuild = (hdr->flags & MAIL_INDEX_HDR_FLAG_FSCKD) != 0;
#endif

  //TODO: check ceph index size, if threshold size has been reached, stat mails
  //      and repair mailbox
  
  ctx = i_new(struct rbox_sync_context, 1);
  ctx->rbox = rbox;
  i_array_init(&ctx->expunged_items, 32);

  int ret = 0;
  if (rebuild) {
    bool success = false;
    for (int i = 0; i < RBOX_REBUILD_COUNT; i++) {
      /* do a full resync and try again. */
      ret = rbox_storage_rebuild_in_context(rbox->storage, force_rebuild, true);
      if (ret >= 0) {
        mailbox_recent_flags_reset(&rbox->box);
        success = true;
        break;
      }
    }
    if (!success || ret < 0) {
      mail_storage_set_critical(storage, "rbox %s: Index keeps breaking", mailbox_get_path(&ctx->rbox->box));
      ret = -1;
    }
  }

  uint8_t sync_flags = index_storage_get_sync_flags(&rbox->box);
  if (!rebuild && (flags & RBOX_SYNC_FLAG_FORCE) == 0)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_REQUIRE_CHANGES;
  if ((flags & RBOX_SYNC_FLAG_FSYNC) != 0)
    sync_flags |= MAIL_INDEX_SYNC_FLAG_FSYNC;
  /* don't write unnecessary dirty flag updates */
  sync_flags |= MAIL_INDEX_SYNC_FLAG_AVOID_FLAG_UPDATES;
  if (ret >= 0) {
    ret = index_storage_expunged_sync_begin(&rbox->box, &ctx->index_sync_ctx, &ctx->sync_view, &ctx->trans,
                                            static_cast<enum mail_index_sync_flags>(sync_flags));
    if (mail_index_reset_fscked(rbox->box.index)){
      // if we set mailbox corrupted then we have a force-resync issue for no reason
      // if we expunged the created index for a appended mail.
      rbox_set_mailbox_corrupted(&rbox->box);
    }
  }
  if (ret <= 0) {
    index_storage_expunging_deinit(&ctx->rbox->box);  
    array_delete(&ctx->expunged_items, array_count(&ctx->expunged_items) - 1, 1);
    array_free(&ctx->expunged_items);
    i_free(ctx);
    *ctx_r = NULL;
    FUNC_END_RET("ret <= 0");
    return ret;
  }
  if (!rebuild) {
    ret = rbox_sync_index(ctx);
  }
  if (ret <= 0) {
    mail_index_sync_rollback(&ctx->index_sync_ctx);
    if (ret < 0) {
      index_storage_expunging_deinit(&ctx->rbox->box);
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

static int rbox_sync_object_expunge(struct rbox_sync_context *ctx, struct expunged_item *item) {
  FUNC_START();
  int ret_remove = -1;
  struct mailbox *box = &ctx->rbox->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  const char *oid = guid_128_to_string(item->oid);

  ret_remove = rbox_open_rados_connection(box, item->alt_storage);
  if (ret_remove < 0) {
    i_error("rbox_sync_object_expunge: connection to rados failed %d, alt_storage(%d), oid(%s)", ret_remove,
            item->alt_storage, oid);
    FUNC_END();
    return ret_remove;
  }
  librmb::RadosStorage *rados_storage = item->alt_storage ? r_storage->alt : r_storage->s;
  ret_remove = rados_storage->get_io_ctx().remove(oid);
  if (ret_remove < 0) {
    if(ret_remove == -ETIMEDOUT) {
      int max_retry = 10;
      for(int i = 0;i<max_retry;i++){
          ret_remove = rados_storage->get_io_ctx().remove(oid);
          if(ret_remove >=0){
            i_error("rbox_sync connection timeout during oid (%s) deletion, mail stays in object store.",oid);
            break;
          }
          i_warning("rbox_sync (retry %d) deletion failed with %d during oid (%s) deletion, mail stays in object store.",i, ret_remove, oid);
          // wait random time before try again!!
          usleep(((rand() % 5) + 1) * 10000);
      }
      
    }
    else if (ret_remove == -ENOENT){
      i_debug("mail oid(%s) already deleted",oid);
    }
    else {
    	i_error("rbox_sync_object_expunge: aio_remove failed with %d oid(%s), alt_storage(%d)", ret_remove, oid,
            item->alt_storage);
    }
  }
 // directly notify
  mailbox_sync_notify(box, item->uid, MAILBOX_SYNC_TYPE_EXPUNGE);    

  FUNC_END();
  return ret_remove;
}

static void rbox_sync_expunge_rbox_objects(struct rbox_sync_context *ctx) {
  FUNC_START();
  struct expunged_item *const *items, *item;
  unsigned int count = 0;

  // rbox_sync_object_expunge;
  items = array_get(&ctx->expunged_items, &count);

  if (count > 0) {
    for (unsigned int i = 0; i < count; i++) {
      T_BEGIN {
        item = items[i];
        rbox_sync_object_expunge(ctx, item);
      }
      T_END;
    }
  }
  mailbox_sync_notify(&ctx->rbox->box, 0, 0);
  
  FUNC_END();
}

int rbox_sync_finish(struct rbox_sync_context **_ctx, bool success) {
  FUNC_START();
  struct rbox_sync_context *ctx = *_ctx;
  int ret = success ? 0 : -1;
  struct expunged_item *const *exp_items, *exp_item;
  unsigned int count = 0;

  *_ctx = NULL;
  if (success) {
    mail_index_view_ref(ctx->sync_view);
    if (mail_index_sync_commit(&ctx->index_sync_ctx) < 0) {
      mailbox_set_index_error(&ctx->rbox->box);
      FUNC_END_RET("ret == -1");
      ret = -1;
    } else {
      // delete/move objects from mailstorage
      rbox_sync_expunge_rbox_objects(ctx);
      // close the view, write changes to index.
      mail_index_view_close(&ctx->sync_view);
    }
  } else {
    mail_index_sync_rollback(&ctx->index_sync_ctx);
  }
  i_info("EXPUNGE: calling deinit");
  index_storage_expunging_deinit(&ctx->rbox->box);

  if (array_is_created(&ctx->expunged_items)) {
    if (array_count(&ctx->expunged_items) > 0) {
      exp_items = array_get(&ctx->expunged_items, &count);
      for (unsigned int i = 0; i < count; i++) {
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

int rbox_sync(struct rbox_mailbox *rbox, enum rbox_sync_flags flags) {
  FUNC_START();

  struct rbox_sync_context *sync_ctx = NULL;

  if (rbox_sync_begin(rbox, &sync_ctx, flags) < 0) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  FUNC_END();
  return sync_ctx == NULL ? 0 : rbox_sync_finish(&sync_ctx, TRUE);
}

struct mailbox_sync_context *rbox_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags) {
  FUNC_START();
  
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
  int ret = 0;

  if (!box->opened) {
    if (mailbox_open(box) < 0) {
      ret = -1;
    }
  }
  
  if (mail_index_reset_fscked(box->index)) {
    rbox_set_mailbox_corrupted(box);
  }
  
  if (ret == 0 && (index_mailbox_want_full_sync(&rbox->box, flags) || rbox->storage->corrupted_rebuild_count != 0)) {
    uint8_t rbox_sync_flags = 0x0;
    if ((flags & MAILBOX_SYNC_FLAG_FORCE_RESYNC) != 0) {
      rbox_sync_flags |= RBOX_SYNC_FLAG_FORCE_REBUILD;
    }
    ret = rbox_sync(rbox, static_cast<enum rbox_sync_flags>(rbox_sync_flags));
  }
  
  FUNC_END();
  return index_mailbox_sync_init(box, flags, ret < 0);
}
