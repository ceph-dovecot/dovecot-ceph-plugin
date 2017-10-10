// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
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

using librmb::RadosMailObject;
using librmb::rbox_metadata_key;

static uint32_t stoui32(const std::string &s) {
  std::istringstream reader(s);
  uint32_t val = 0;
  reader >> val;
  return val;
}

int rbox_sync_add_object(struct index_rebuild_context *ctx, const std::string &oi, librmb::RadosMailObject *mail_obj) {
  uint32_t seq;
  struct rbox_mailbox *rbox_mailbox = (struct rbox_mailbox *)ctx->box;
  std::string xattr_mail_uid = mail_obj->get_xvalue(rbox_metadata_key::RBOX_METADATA_MAIL_UID);
  // char *xattr_mail_uid = get_xattr_value(attrset, RBOX_METADATA_MAIL_UID);
  if (xattr_mail_uid.empty()) {
    i_debug("xattr_mail_uid.empty");
    return -1;
  }

  std::string xattr_guid = mail_obj->get_xvalue(rbox_metadata_key::RBOX_METADATA_GUID);
  if (xattr_guid.empty()) {
    i_debug("xattr_guid empty");
    return -1;
  }

  uint32_t uid = stoui32(xattr_mail_uid);
  mail_index_append(ctx->trans, uid, &seq);
  i_debug("added to index %d", seq);

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  i_zero(&rec);
  // convert oid and guid to
  guid_128_t oid;
  if (guid_128_from_string(oi.c_str(), oid) < 0) {
    i_debug("guid_128 oi.c_str() string %s", oi.c_str());
    return -1;
  }
  guid_128_t guid;
  if (guid_128_from_string(xattr_guid.c_str(), guid) < 0) {
    i_debug("guid_128 xattr_guid string %s", xattr_guid.c_str());

    return -1;
  }
  memcpy(rec.guid, guid, sizeof(guid));
  memcpy(rec.oid, oid, sizeof(oid));

  i_debug("rebuilding....");
  mail_index_update_ext(ctx->trans, seq, rbox_mailbox->ext_id, &rec, NULL);

  T_BEGIN { index_rebuild_index_metadata(ctx, seq, uid); }
  T_END;
  i_debug("rebuilding %s , with uid=%d", oi.c_str(), uid);

  return 0;
}
// find objects with mailbox_guid 'U' attribute
int rbox_sync_index_rebuild(struct index_rebuild_context *ctx, librados::NObjectIterator &iter) {
  struct mail_storage *storage = ctx->box->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;

  // find all objects with x attr M = mailbox_guid
  // if non is found : set mailbox_deleted and mail_storage_set_critical...

  int found = 0;
  int ret = 0;
  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    std::map<std::string, ceph::bufferlist> attrset;
    librmb::RadosMailObject mail_object;
    mail_object.set_oid((*iter).get_oid());

    int retx = r_storage->s->load_metadata(&mail_object);

    if (retx >= 0) {
      ret = rbox_sync_add_object(ctx, (*iter).get_oid(), &mail_object);
      if (ret < 0) {
        break;
      }
    }
    ++iter;
    ++found;
  }
  if (ret < 0) {
    i_debug("error rbox_sync_add_objects for mbox %s", ctx->box->name);
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

int rbox_sync_index_rebuild_objects(struct index_rebuild_context *ctx) {
  int ret = 0;
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)ctx->box;
  struct mail_storage *storage = ctx->box->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;
  rbox_sync_set_uidvalidity(ctx);

  if (rbox_open_rados_connection(ctx->box) < 0) {
    i_debug("connection not valid");
    return -1;
  }

  std::string guid(guid_128_to_string(rbox->mailbox_guid));
  i_debug("guid is empty, using mailbox name to detect mail objects");
  librmb::RadosMetadata attr_guid(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID, guid);
  // rebuild index.

  librados::NObjectIterator iter_guid(r_storage->s->find_mails(&attr_guid));
  if (iter_guid != librados::NObjectIterator::__EndObjectIterator) {
    ret = rbox_sync_index_rebuild(ctx, iter_guid);
  } else {
    librmb::RadosMetadata attr_name(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, rbox->box.name);
    // rebuild index.
    librados::NObjectIterator iter_name(r_storage->s->find_mails(&attr_name));
    ret = rbox_sync_index_rebuild(ctx, iter_name);
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
  i_warning("rbox %s: Rebuilding index, guid: %s , mailbox_name: %s", mailbox_get_path(&mbox->box),
            guid_128_to_string(mbox->mailbox_guid), mbox->box.name);

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
