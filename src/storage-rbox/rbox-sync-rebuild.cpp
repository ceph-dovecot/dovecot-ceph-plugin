// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

extern "C" {
#include "dovecot-all.h"

#include "rbox-sync.h"
#include "debug-helper.h"
}

#include "rbox-sync-rebuild.h"

#include "rbox-storage.hpp"
#include "rbox-mail.h"
#include "encoding.h"
#include "rados-mail-object.h"
#include "rados-util.h"

using librmb::RadosMailObject;
using librmb::rbox_metadata_key;

static uint32_t stoui32(const std::string &s) {
  std::istringstream reader(s);
  uint32_t val = 0;
  reader >> val;
  return val;
}

int rbox_sync_add_object(struct index_rebuild_context *ctx, const std::string &oi, librmb::RadosMailObject *mail_obj,
                         bool alt_storage, bool update_uid, uint32_t uid) {
  uint32_t seq;
  struct rbox_mailbox *rbox_mailbox = (struct rbox_mailbox *)ctx->box;
  std::string xattr_mail_uid = mail_obj->get_metadata(rbox_metadata_key::RBOX_METADATA_MAIL_UID);
  std::string xattr_guid = mail_obj->get_metadata(rbox_metadata_key::RBOX_METADATA_GUID);
  struct mail_storage *storage = ctx->box->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;

  if (!update_uid) {
    // use xattr uid
    uid = stoui32(xattr_mail_uid);
  }

  mail_index_append(ctx->trans, uid, &seq);
  i_debug("added to index %d", seq);

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  i_zero(&rec);
  // convert oid and guid to
  guid_128_t oid;
  if (guid_128_from_string(oi.c_str(), oid) < 0) {
    i_error("guid_128 oi.c_str() string %s", oi.c_str());
    return -1;
  }
  guid_128_t guid;
  if (guid_128_from_string(xattr_guid.c_str(), guid) < 0) {
    i_error("guid_128 xattr_guid string '%s'", xattr_guid.c_str());
    return -1;
  }
  memcpy(rec.guid, guid, sizeof(guid));
  memcpy(rec.oid, oid, sizeof(oid));

  mail_index_update_ext(ctx->trans, seq, rbox_mailbox->ext_id, &rec, NULL);
  if (alt_storage) {
    mail_index_update_flags(ctx->trans, seq, MODIFY_ADD, (enum mail_flags)RBOX_INDEX_FLAG_ALT);
  }

  T_BEGIN { index_rebuild_index_metadata(ctx, seq, uid); }
  T_END;

  if (update_uid) {
    // update uid.
    librmb::RadosMetadata mail_uid(librmb::RBOX_METADATA_MAIL_UID, uid);
    std::string oid = mail_obj->get_oid();
    std::list<librmb::RadosMetadata> to_update;
    to_update.push_back(mail_uid);
    if (r_storage->ms->get_storage()->update_metadata(oid, to_update) < 0) {
      i_warning("update of MAIL_UID failed: for object: %s , uid: %d", mail_obj->get_oid().c_str(), uid);
    }
  }
  i_debug("rebuilding %s , with oid=%d", oi.c_str(), uid);

  return 0;
}
// find objects with mailbox_guid 'U' attribute
int rbox_sync_rebuild_entry(struct index_rebuild_context *ctx, librados::NObjectIterator &iter, bool alt_storage,
                            bool generate_guid) {
  struct mail_storage *storage = ctx->box->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;

  // find all objects with x attr M = mailbox_guid
  // if non is found : set mailbox_deleted and mail_storage_set_critical...

  // it is required to check for duplicat uids,
  // because if user moved / copied mails from mailbox to sub-mailbox,
  // uid may stay the same (depends on configuration).
  std::vector<std::string> uids;

  int found = 0;
  int ret = 0;
  uint32_t uid = 1;
  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    std::map<std::string, ceph::bufferlist> attrset;
    librmb::RadosMailObject mail_object;
    mail_object.set_oid((*iter).get_oid());
    int retx;
    if (alt_storage) {
      r_storage->ms->get_storage()->set_io_ctx(&r_storage->alt->get_io_ctx());
      retx = r_storage->ms->get_storage()->load_metadata(&mail_object);
    } else {
      retx = r_storage->ms->get_storage()->load_metadata(&mail_object);
    }

    if (!librmb::RadosUtils::validate_metadata(mail_object.get_metadata())) {
      i_error("metadata for object : %s is not valid, skipping object ", mail_object.get_oid().c_str());
      ++iter;
      continue;
    }
    if (retx >= 0) {
      ret = rbox_sync_add_object(ctx, (*iter).get_oid(), &mail_object, alt_storage, generate_guid, uid);
      if (ret < 0) {
        break;
      }
    }
    ++iter;
    ++found;
    if (generate_guid) {
      ++uid;
    }
  }
  if (ret < 0) {
    i_error("error rbox_sync_add_objects for mbox %s", ctx->box->name);
    mailbox_set_deleted(ctx->box);
    mail_storage_set_critical(storage, "find mailbox(%s) failed: %m", ctx->box->name);
    return -1;
  }

  if (found == 0) {
    i_debug("no entry to restore can be found for mailbox %s", ctx->box->name);
    mailbox_set_deleted(ctx->box);
    return 0;
  }

  return ret;
}

void rbox_sync_set_uidvalidity(struct index_rebuild_context *ctx) {
  uint32_t uid_validity;

  /* if uidvalidity is set in the old index, use it */
  uid_validity = mail_index_get_header(ctx->view)->uid_validity;
  if (uid_validity == 0)
    uid_validity = rbox_get_uidvalidity_next(ctx->box->list);

  mail_index_update_header(ctx->trans, offsetof(struct mail_index_header, uid_validity), &uid_validity,
                           sizeof(uid_validity), TRUE);
}

int search_objects(struct index_rebuild_context *ctx, bool alt_storage) {
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)ctx->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)ctx->box->storage;
  librmb::RadosStorage *storage = alt_storage ? r_storage->alt : r_storage->s;
  int ret = 0;
  std::string guid(guid_128_to_string(rbox->mailbox_guid));
  librmb::RadosMetadata attr_guid(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID, guid);
  // rebuild index.

  bool generate_guid = false;
  librados::NObjectIterator iter_guid(storage->find_mails(&attr_guid));
  if (iter_guid != librados::NObjectIterator::__EndObjectIterator) {
    ret = rbox_sync_rebuild_entry(ctx, iter_guid, alt_storage, generate_guid);
  } else {
    i_debug("guid is empty, using mailbox name to detect mail objects ");
    librmb::RadosMetadata attr_name(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, rbox->box.name);
    // rebuild index.
    librados::NObjectIterator iter_name(storage->find_mails(&attr_name));
    generate_guid = true;
    ret = rbox_sync_rebuild_entry(ctx, iter_name, alt_storage, generate_guid);
  }
  return ret;
}

int rbox_sync_index_rebuild_objects(struct index_rebuild_context *ctx) {
  int ret = 0;
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)ctx->box;
  rbox_sync_set_uidvalidity(ctx);

  bool alt_storage = is_alternate_pool_valid(ctx->box);

  if (rbox_open_rados_connection(ctx->box, alt_storage) < 0) {
    i_error("cannot open rados connection");
    return -1;
  }

  search_objects(ctx, false);
  if (alt_storage) {
    i_debug("ALT_STORAGE ACTIVE: '%s' ", rbox->box.list->set.alt_dir);
    search_objects(ctx, true);
  }

  rbox_sync_update_header(ctx);
  return ret;
}

int rbox_sync_index_rebuild(struct rbox_mailbox *mbox, bool force) {
  struct index_rebuild_context *ctx;
  struct mail_index_view *view;
  struct mail_index_transaction *trans;
  struct sdbox_index_header hdr;
  bool need_resize;
  int ret;

  // get mailbox guid
  if (!force && rbox_read_header(mbox, &hdr, FALSE, &need_resize) == 0) {
    if (hdr.rebuild_count != mbox->corrupted_rebuild_count && hdr.rebuild_count != 0) {
      /* already rebuilt by someone else */
      return 0;
    }
    i_debug("index could not be opened");
    // try to determine mailbox guid via xattr.
  }
  i_warning("rbox %s: Rebuilding index, guid: %s , mailbox_name: %s, alt_storage: %s", mailbox_get_path(&mbox->box),
            guid_128_to_string(mbox->mailbox_guid), mbox->box.name, mbox->box.list->set.alt_dir);

  view = mail_index_view_open(mbox->box.index);

  trans = mail_index_transaction_begin(view, MAIL_INDEX_TRANSACTION_FLAG_EXTERNAL);

  ctx = index_index_rebuild_init(&mbox->box, view, trans);

  ret = rbox_sync_index_rebuild_objects(ctx);

  i_debug("rebuild finished");
  index_index_rebuild_deinit(&ctx, rbox_get_uidvalidity_next);

  if (ret < 0) {
    mail_index_transaction_rollback(&trans);
  } else {
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_MAIL_INDEX_HDR_FLAG_FSCKD
    mail_index_unset_fscked(trans);
#endif
    ret = mail_index_transaction_commit(&trans);
  }
  mail_index_view_close(&view);
  mbox->corrupted_rebuild_count = 0;
  return ret;
}
