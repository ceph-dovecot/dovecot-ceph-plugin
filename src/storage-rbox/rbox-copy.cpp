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

#include <ctime>
#include <string>

extern "C" {

#include "dovecot-all.h"
#include "istream.h"
#include "mail-copy.h"

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

  i_debug("rbox_mail_copy: copying = %s", btoa(ctx->copying));
  int ret = rbox_mail_storage_copy(_ctx, mail);
  ctx->copying = FALSE;

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

static void set_mailbox_xattr(struct mail_save_context *ctx, librados::ObjectWriteOperation *write_op) {
  {
    // #ifdef < 2.2.26
    struct mailbox *dest_mbox = ctx->transaction->box;
    // else
    // struct rbox_mailbox *dest_mbox =ctx->dest_mail->box;
    // #endif
    struct rbox_mailbox *dest_mailbox = (struct rbox_mailbox *)(dest_mbox);
    librmb::RadosXAttr xattr(
        rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID,
        guid_128_to_string(dest_mailbox->mailbox_guid));
    write_op->setxattr(xattr.key.c_str(), xattr.bl);
    i_debug("setting orig mailbox_name %s", dest_mailbox->box.name);
    std::string update_immutable = SETTINGS_DEF_UPDATE_IMMUTABLE;
    const char *setting_update_immutable = mail_user_plugin_getenv(
        dest_mailbox->storage->storage.user, SETTINGS_RBOX_UPDATE_IMMUTABLE);
    if (setting_update_immutable != nullptr
        && strlen(setting_update_immutable) > 0) {
      update_immutable = setting_update_immutable;
    }
    if (update_immutable.compare("true") == 0) {
    librmb::RadosXAttr xattr_mb(
        rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, dest_mailbox->box.name);
    write_op->setxattr(xattr_mb.key.c_str(), xattr_mb.bl);
    }
    i_debug("setting done");
  }
}

static int rbox_mail_storage_try_copy(struct mail_save_context **_ctx, struct mail *mail) {
  FUNC_START();
  struct mail_save_context *ctx = *_ctx;
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;
  struct rbox_mail *rmail = (struct rbox_mail *)mail;
  struct rbox_mailbox *rmailbox = (struct rbox_mailbox *)mail->box;
  librados::IoCtx src_io_ctx;

  // #ifdef < 2.2.26
  struct mailbox *dest_mbox = ctx->transaction->box;
  // else
  // struct rbox_mailbox *dest_mbox =ctx->dest_mail->box;
  // #endif

  librados::IoCtx dest_io_ctx = r_storage->s->get_io_ctx();
  const char *ns_src_mail = mail->box->list->ns->owner != nullptr ? mail->box->list->ns->owner->username : "";
  const char *ns_dest_mail = dest_mbox->list->ns->owner != nullptr ? dest_mbox->list->ns->owner->username : "";

  i_debug("rbox_mail_storage_try_copy: mail = %p", mail);
  int ret_val = 0;

  if (r_ctx->copying == TRUE) {
    i_debug("namespace src %s, namespace dest %s", ns_src_mail, ns_dest_mail);

    if (rbox_get_index_record(mail) < 0) {
      rbox_mail_copy_set_failed(ctx, mail, "index record");
      FUNC_END_RET("ret == -1, rbox_get_index_record failed");
      return -1;
    }

    if (rbox_mail_save_copy_default_metadata(ctx, mail) < 0) {
      FUNC_END_RET("ret == -1, mail_save_copy_default_metadata failed");
      return -1;
    }

    librados::ObjectWriteOperation write_op;
    librados::AioCompletion *completion = librados::Rados::aio_create_completion();

    if (ctx->moving != TRUE) {
      rbox_add_to_index(ctx);
      std::string src_oid = rmail->mail_object->get_oid();
      std::string dest_oid = r_ctx->current_object->get_oid();
      i_debug("rbox_mail_storage_try_copy: from source %s to dest %s", src_oid.c_str(), dest_oid.c_str());

      if (strcmp(ns_src_mail, ns_dest_mail) != 0) {
        src_io_ctx.dup(dest_io_ctx);
        src_io_ctx.set_namespace(ns_src_mail);
        dest_io_ctx.set_namespace(ns_dest_mail);
      } else {
        src_io_ctx = dest_io_ctx;
      }
      i_debug("ns_compare: %s %s", ns_src_mail, ns_dest_mail);
      write_op.copy_from(src_oid, src_io_ctx, src_io_ctx.get_last_version());

      // because we create a copy, save date needs to be updated
      // as an alternative we could use &ctx->data.save_date here if we save it to xattribute in write_metadata
      // and restore it in read_metadata function. => save_date of copy/move will be same as source.
      // write_op.mtime(&ctx->data.save_date);
      time_t save_time = time(NULL);
      write_op.mtime(&save_time);

      i_debug("cpy_time: oid: %s, save_date: %s", src_oid.c_str(), std::ctime(&save_time));
      {
        struct rbox_mailbox *dest_mailbox = (struct rbox_mailbox *)dest_mbox;
        librmb::RadosXAttr xattr(
            rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID,
            guid_128_to_string(dest_mailbox->mailbox_guid));
        write_op.setxattr(xattr.key.c_str(), xattr.bl);

        std::string update_immutable = SETTINGS_DEF_UPDATE_IMMUTABLE;
        const char *setting_update_immutable =
            mail_user_plugin_getenv(dest_mailbox->storage->storage.user, SETTINGS_RBOX_UPDATE_IMMUTABLE);
        if (setting_update_immutable != nullptr && strlen(setting_update_immutable) > 0) {
          update_immutable = setting_update_immutable;
        }

        if (update_immutable.compare("true") == 0) {
          i_debug("setting orig mailbox_name %s", dest_mailbox->box.name);
          librmb::RadosXAttr xattr_mb(
              rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX,
              dest_mailbox->box.name);
          write_op.setxattr(xattr_mb.key.c_str(), xattr_mb.bl);
          i_debug("setting done");
        }
      }

      ret_val = r_storage->s->aio_operate(&dest_io_ctx, dest_oid, completion, &write_op);

      i_debug("copy finished: oid = %s, ret_val = %d, mtime = %ld", dest_oid.c_str(), ret_val, save_time);

    } else {
      struct expunged_item *item = p_new(default_pool, struct expunged_item, 1);
      std::string src_oid = rmail->mail_object->get_oid();

      guid_128_from_string(src_oid.c_str(), item->oid);
      array_append(&rmailbox->moved_items, &item, 1);

      rbox_move_index(ctx, mail);

      set_mailbox_xattr(ctx, &write_op);

      // int err = dest_io_ctx.aio_operate(src_oid, completion, &write_op);
      int err = r_storage->s->aio_operate(&dest_io_ctx, src_oid, completion, &write_op);

      i_debug("move finished: oid = %s, ret_val = %d", src_oid.c_str(), err);
    }

    index_copy_cache_fields(ctx, mail, r_ctx->seq);
    if (ctx->dest_mail != NULL) {
      mail_set_seq_saving(ctx->dest_mail, r_ctx->seq);
    }
    completion->wait_for_complete();
    completion->release();
    // reset io_ctx
    dest_io_ctx.set_namespace(ns_src_mail);
  }
  FUNC_END();
  return ret_val;
}

int rbox_mail_storage_copy(struct mail_save_context *ctx, struct mail *mail) {
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)ctx;

  // #ifdef < 2.2.26
  struct mailbox *dest_mbox = ctx->transaction->box;
  // else
  // struct rbox_mailbox *dest_mbox =ctx->dest_mail->box;
  // #endif

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

  if (rbox_open_rados_connection(dest_mbox) < 0) {
    FUNC_END_RET("ret == -1, connection to rados failed");
    return -1;
  }

  if (rbox_mail_storage_try_copy(&ctx, mail) < 0) {
    if (ctx != NULL)
      mailbox_save_cancel(&ctx);
    FUNC_END_RET("ret == -1, rbox_mail_storage_try_copy failed");
    return -1;
  }
  FUNC_END();

  return mail_storage_copy(ctx, mail);
}
