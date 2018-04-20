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

#include <ctime>
#include <list>
#include <string>

extern "C" {

#include "dovecot-all.h"
#include "istream.h"
#include "mail-copy.h"
#include "ostream.h"
#include "ostream-private.h"
#include "debug-helper.h"
}
#include "rados-mail-object.h"
#include "rbox-storage.hpp"
#include "rbox-mail.h"
#include "rbox-save.h"
#include "rbox-sync.h"
#include "rbox-copy.h"

const char *SETTINGS_RBOX_UPDATE_IMMUTABLE = "rbox_update_immutable";
const char *SETTINGS_DEF_UPDATE_IMMUTABLE = "false";

using librmb::rbox_metadata_key;

int rbox_mail_storage_copy(struct mail_save_context *ctx, struct mail *mail);

int rbox_mail_copy(struct mail_save_context *_ctx, struct mail *mail) {
  FUNC_START();

  struct rbox_mailbox *dest_mbox = (struct rbox_mailbox *)_ctx->transaction->box;
  const char *storage_name = dest_mbox->storage->storage.name;
  struct rbox_save_context *ctx = (struct rbox_save_context *)_ctx;

  ctx->copying = _ctx->saving != TRUE && strcmp(mail->box->storage->name, "rbox") == 0 &&
                 strcmp(mail->box->storage->name, storage_name) == 0;

  int ret = rbox_mail_storage_copy(_ctx, mail);
  ctx->copying = FALSE;

  index_save_context_free(_ctx);

  FUNC_END();
  return ret;
}

static void rbox_mail_copy_set_failed(struct mail_save_context *ctx, struct mail *mail, const char *func) {
  const char *errstr;
  enum mail_error error;

  if (ctx->transaction->box->storage == mail->box->storage)
    return;

  errstr = mail_storage_get_last_error(mail->box->storage, &error);
  mail_storage_set_error(ctx->transaction->box->storage, error, t_strdup_printf("%s (%s)", errstr, func));
}

static int rbox_mail_save_copy_default_metadata(struct mail_save_context *ctx, struct mail *mail) {
  FUNC_START();
  const char *from_envelope, *guid;
  time_t received_date;

  if (ctx->data.received_date == (time_t)-1) {
    if (mail_get_received_date(mail, &received_date) < 0) {
      rbox_mail_copy_set_failed(ctx, mail, "received-date");
      FUNC_END_RET("ret == -1, mail_get_received_date failed");
      return -1;
    }
    mailbox_save_set_received_date(ctx, received_date, 0);
  }
  if (ctx->data.from_envelope == NULL) {
    if (mail_get_special(mail, MAIL_FETCH_FROM_ENVELOPE, &from_envelope) < 0) {
      rbox_mail_copy_set_failed(ctx, mail, "from-envelope");
      FUNC_END_RET("ret == -1, mail_get_special envelope failed");
      return -1;
    }
    if (*from_envelope != '\0')
      mailbox_save_set_from_envelope(ctx, from_envelope);
  }
  if (ctx->data.guid == NULL) {
    if (mail_get_special(mail, MAIL_FETCH_GUID, &guid) < 0) {
      rbox_mail_copy_set_failed(ctx, mail, "guid");
      FUNC_END_RET("ret == -1, mail_get_special guid failed");
      return -1;
    }
    if (*guid != '\0')
      mailbox_save_set_guid(ctx, guid);
  }

  FUNC_END();
  return 0;
}

static void set_mailbox_metadata(struct mail_save_context *ctx, std::list<librmb::RadosMetadata> *metadata_update) {
  {
    struct mailbox *dest_mbox = ctx->transaction->box;
    struct rbox_save_context *r_ctx = (struct rbox_save_context *)ctx;
    struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

    struct rbox_mailbox *dest_mailbox = (struct rbox_mailbox *)(dest_mbox);

    // #130 always update Mailbox guid, in case we need to resync the mailbox!
    librmb::RadosMetadata metadata_mailbox_guid(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID,
                                                guid_128_to_string(dest_mailbox->mailbox_guid));
    metadata_update->push_back(metadata_mailbox_guid);

    if (r_storage->config->is_update_attributes()) {
      if (r_storage->config->is_updateable_attribute(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX)) {
        // updates the plain text mailbox name
        librmb::RadosMetadata metadata_mbn(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, dest_mailbox->box.name);
        metadata_update->push_back(metadata_mbn);
      }
    }
  }
}


static int rbox_mail_storage_try_copy(struct mail_save_context **_ctx, struct mail *mail) {
  FUNC_START();
  struct mail_save_context *ctx = *_ctx;
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;
  struct rbox_mail *rmail = (struct rbox_mail *)mail;
  struct rbox_mailbox *rmailbox = (struct rbox_mailbox *)mail->box;

  struct mailbox *dest_mbox = ctx->transaction->box;

  std::string ns_src_mail1;

  if (mail->box->list->ns->owner != nullptr) {
    ns_src_mail1 = mail->box->list->ns->owner->username;
    ns_src_mail1 += r_storage->config->get_user_suffix();
  } else {
    ns_src_mail1 = r_storage->config->get_public_namespace();
  }

  std::string ns_dest_mail1;
  if (dest_mbox->list->ns->owner != nullptr) {
    ns_dest_mail1 = dest_mbox->list->ns->owner->username;
    ns_dest_mail1 += r_storage->config->get_user_suffix();
  } else {
    ns_dest_mail1 = r_storage->config->get_public_namespace();
  }

  std::string ns_src;
  if (!r_storage->ns_mgr->lookup_key(ns_src_mail1, &ns_src)) {
    i_error("not initialized : ns_src");
    return -1;
  }
  //  ns_src = "raw mail user";
  std::string ns_dest;
  if (!r_storage->ns_mgr->lookup_key(ns_dest_mail1, &ns_dest)) {
    i_error("not_initialized : ns_dest");
    return -1;
  }

  i_debug("namespaces: src=%s, dst=%s", ns_src.c_str(), ns_dest.c_str());

  int ret_val = 0;

  if (r_ctx->copying == TRUE) {
    i_debug("using copying = TRUE");
    if (rbox_get_index_record(mail) < 0) {
      rbox_mail_copy_set_failed(ctx, mail, "index record");
      FUNC_END_RET("ret == -1, rbox_get_index_record failed");
      return -1;
    }

    if (rbox_mail_save_copy_default_metadata(ctx, mail) < 0) {
      FUNC_END_RET("ret == -1, mail_save_copy_default_metadata failed");
      return -1;
    }

    std::list<librmb::RadosMetadata> metadata_update;  // metadata which needs to be unique goes here
    std::string src_oid = rmail->mail_object->get_oid();

    if (ctx->moving != TRUE) {
      rbox_add_to_index(ctx);

      std::string dest_oid = r_ctx->current_object->get_oid();

      set_mailbox_metadata(ctx, &metadata_update);

      if (!r_storage->s->copy(src_oid, ns_src.c_str(), dest_oid, ns_dest.c_str(), metadata_update)) {
        i_error("copy mail failed: from namespace: %s to namespace %s: src_oid: %s, des_oid: %s", ns_src.c_str(),
                ns_dest.c_str(), src_oid.c_str(), dest_oid.c_str());
        FUNC_END_RET("ret == -1, rados_storage->copy failed");
        return -1;
      }

      i_debug("copy successfully finished: from src %s to oid = %s", src_oid.c_str(), dest_oid.c_str());
    }
    if (ctx->moving) {
      rbox_move_index(ctx, mail);

      std::string dest_oid = src_oid;

      set_mailbox_metadata(ctx, &metadata_update);

      // set src as expunged
      struct expunged_item *item = p_new(default_pool, struct expunged_item, 1);
      guid_128_from_string(src_oid.c_str(), item->oid);
      array_append(&rmailbox->moved_items, &item, 1);

      bool delete_source = true;
      if (!r_storage->s->move(src_oid, ns_src.c_str(), dest_oid, ns_dest.c_str(), metadata_update, delete_source)) {
        i_error("move mail failed: from namespace: %s to namespace %s: src_oid: %s, des_oid: %s", ns_src.c_str(),
                ns_dest.c_str(), src_oid.c_str(), dest_oid.c_str());
        FUNC_END_RET("ret == -1, rados_storage->move failed");
        return -1;
      }

      i_debug("move successfully finished from %s (ns=%s) to %s (ns=%s)", src_oid.c_str(), ns_src.c_str(),
              src_oid.c_str(), ns_dest.c_str());
    }
    index_copy_cache_fields(ctx, mail, r_ctx->seq);
    if (ctx->dest_mail != NULL) {
      mail_set_seq_saving(ctx->dest_mail, r_ctx->seq);
    }
  }
  FUNC_END();
  return ret_val;
}

int rbox_mail_storage_copy(struct mail_save_context *ctx, struct mail *mail) {
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)ctx;

  struct mailbox *dest_mbox = ctx->transaction->box;

  FUNC_START();
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_COPYING_OR_MOVING
  i_assert(ctx->copying_or_moving);
#endif
  r_ctx->finished = TRUE;

  if (ctx->data.keywords != NULL) {
    /* keywords gets unreferenced twice: first in
       mailbox_save_cancel()/_finish() and second time in
       mailbox_copy(). */
    mailbox_keywords_ref(ctx->data.keywords);
  }

  if (ctx->saving || !r_ctx->copying || strcmp(mail->box->storage->name, "rbox") != 0) {
    // LDA or doveadm backup need copy for saving the mail

    // explicitly copy flags
    mailbox_save_copy_flags(ctx, mail);

    if (mail_storage_copy(ctx, mail) < 0) {
      FUNC_END_RET("ret == -1, mail_storage_copy failed");
      return -1;
    } else {
      i_debug("Mail saved without ceph copy, uid = %u, oid = %s", mail->uid, guid_128_to_string(r_ctx->mail_oid));
    }

  } else {
    if (rbox_open_rados_connection(dest_mbox) < 0) {
      FUNC_END_RET("ret == -1, connection to rados failed");
      return -1;
    }

    if (rbox_mail_storage_try_copy(&ctx, mail) < 0) {
      if (ctx != NULL) {
        mailbox_save_cancel(&ctx);
      }
      FUNC_END_RET("ret == -1, rbox_mail_storage_try_copy failed ");
      return -1;
    }
  }

  ctx->unfinished = false;

  FUNC_END();
  return 0;
}
