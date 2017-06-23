/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <stdio.h>
#include <utime.h>

#include <string>
#include <map>
#include <vector>

#include <rados/librados.hpp>

extern "C" {
#include "lib.h"
#include "typeof-def.h"

#include "array.h"
#include "hostpid.h"
#include "istream.h"
#include "istream-crlf.h"
#include "ostream.h"
#include "str.h"
#include "index-mail.h"
#include "rbox-storage.h"
#include "rbox-sync.h"
#include "debug-helper.h"
}

#include "rados-mail-object.h"
#include "rbox-storage-struct.h"
#include "rados-storage.h"
#include "rbox-save.h"

using namespace librados;  // NOLINT
using namespace librmb;    // NOLINT
using namespace ceph;      // NOLINT

using std::string;

struct mail_save_context *rbox_save_alloc(struct mailbox_transaction_context *t) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)t->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)mbox->storage;
  rbox_save_context *r_ctx;

  i_assert((t->flags & MAILBOX_TRANSACTION_FLAG_EXTERNAL) != 0);

  if (t->save_ctx == NULL) {
    i_debug("rbox_save_alloc: t->save_ctx == NULL, mailbox name = %s", t->box->name);
    r_ctx = new rbox_save_context(*(r_storage->s));
    r_ctx->ctx.transaction = t;
    r_ctx->mbox = mbox;
    r_ctx->trans = t->itrans;
    t->save_ctx = &r_ctx->ctx;
  }
  debug_print_mail_save_context(t->save_ctx, "rbox-save::rbox_save_alloc", NULL);

  FUNC_END();
  return t->save_ctx;
}

void rbox_add_to_index(struct mail_save_context *_ctx) {
  struct mail_save_data *mdata = &_ctx->data;
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  enum mail_flags save_flags;

  /* add to index */
  save_flags = mdata->flags & ~MAIL_RECENT;
  mail_index_append(r_ctx->trans, 0, &r_ctx->seq);
  mail_index_update_flags(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE, static_cast<mail_flags>(save_flags));
  if (_ctx->data.keywords != NULL) {
    mail_index_update_keywords(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE, _ctx->data.keywords);
  }
  if (_ctx->data.min_modseq != 0) {
    mail_index_update_modseq(r_ctx->trans, r_ctx->seq, _ctx->data.min_modseq);
  }

  mail_set_seq_saving(_ctx->dest_mail, r_ctx->seq);

  guid_128_generate(r_ctx->mail_oid);

  r_ctx->current_object = new RadosMailObject();
  r_ctx->current_object->set_oid(guid_128_to_string(r_ctx->mail_oid));

  if (mdata->guid != NULL) {
    mail_generate_guid_128_hash(mdata->guid, r_ctx->mail_guid);
  } else {
    guid_128_generate(r_ctx->mail_guid);
  }

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  i_zero(&rec);
  memcpy(rec.guid, r_ctx->mail_guid, sizeof(r_ctx->mail_guid));
  memcpy(rec.oid, r_ctx->mail_oid, sizeof(r_ctx->mail_oid));
  i_debug("SAVE OID: %s", guid_128_to_string(rec.oid));
  mail_index_update_ext(r_ctx->trans, r_ctx->seq, r_ctx->mbox->ext_id, &rec, NULL);
}

int rbox_save_begin(struct mail_save_context *_ctx, struct istream *input) {
  FUNC_START();
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  i_debug(" rbox_save_begin");

  struct mailbox_transaction_context *trans = _ctx->transaction;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;
  struct istream *crlf_input;

  r_ctx->failed = FALSE;

  if (r_ctx->copying != TRUE) {
    rbox_add_to_index(_ctx);
    crlf_input = i_stream_create_crlf(input);
    r_ctx->input = index_mail_cache_parse_init(_ctx->dest_mail, crlf_input);
    i_stream_unref(&crlf_input);
  }

  r_ctx->mail_buffer = buffer_create_dynamic(default_pool, 1014);
  if (r_ctx->mail_buffer == NULL) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  if (_ctx->data.output != NULL) {
    o_stream_unref(&_ctx->data.output);
  }
  _ctx->data.output = o_stream_create_buffer(r_ctx->mail_buffer);

  bufferlist version_bl;
  version_bl.append(RadosMailObject::X_ATTR_VERSION_VALUE);

  r_ctx->current_object->get_write_op().setxattr(RadosMailObject::X_ATTR_VERSION.c_str(), version_bl);
  if (r_ctx->failed) {
    debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_begin (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_begin", NULL);

  FUNC_END();
  return r_ctx->failed ? -1 : 0;
}

int rbox_save_continue(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;

  if (r_ctx->copying == TRUE) {
    return 0;
  }

  if (r_ctx->failed) {
    debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_continue (ret -1, 1)", NULL);
    debug_print_mail_storage(storage, "rbox-save::rbox_save_continue (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  do {
    if (o_stream_send_istream(_ctx->data.output, r_ctx->input) < 0) {
      if (!mail_storage_set_error_from_errno(storage)) {
        mail_storage_set_critical(storage, "write(%s) failed: %m", "unused JAR");
      }
      r_ctx->failed = TRUE;
      debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_continue (ret -1, 2)", NULL);
      debug_print_mail_storage(storage, "rbox-save::rbox_save_continue (ret -1, 2)", NULL);
      FUNC_END_RET("ret == -1");
      return -1;
    }
    index_mail_cache_parse_continue(_ctx->dest_mail);

    /* both tee input readers may consume data from our primary
     input stream. we'll have to make sure we don't return with
     one of the streams still having data in them. */
  } while (i_stream_read(r_ctx->input) > 0);

  debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_continue", NULL);
  debug_print_mail_storage(storage, "rbox-save::rbox_save_continue", NULL);
  FUNC_END();
  return 0;
}

static int rbox_save_mail_write_metadata(struct rbox_save_context *ctx) {
  FUNC_START();
  struct rbox_mailbox *mbox = ctx->mbox;
  struct mail_storage *storage = ctx->ctx.transaction->box->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;
  struct mail_save_data *mdata = &ctx->ctx.data;

  {
    bufferlist bl;
    bl.append(guid_128_to_string(ctx->mail_guid));
    ctx->current_object->get_write_op().setxattr(RadosMailObject::X_ATTR_GUID.c_str(), bl);
  }
  {
    bufferlist bl;
    bl.append(mdata->save_date);
    ctx->current_object->get_write_op().setxattr(RadosMailObject::X_ATTR_SAVE_DATE.c_str(), bl);
  }

  if (mdata->pop3_uidl != NULL) {
    i_assert(strchr(mdata->pop3_uidl, '\n') == NULL);
    bufferlist bl;
    bl.append(mdata->pop3_uidl);
    ctx->current_object->get_write_op().setxattr(RadosMailObject::X_ATTR_POP3_UIDL.c_str(), bl);
  }

  if (mdata->pop3_order != 0) {
    bufferlist bl;
    bl.append(mdata->pop3_order);
    ctx->current_object->get_write_op().setxattr(RadosMailObject::X_ATTR_POP3_ORDER.c_str(), bl);
  }

  ctx->current_object->get_write_op().mtime(&mdata->received_date);

  FUNC_END();
  return 0;
}

static void remove_from_rados(librmb::RadosStorage *_storage, const std::string &_oid) {
  i_debug("object to delete oid is: %s", _oid.c_str());
  if ((_storage->get_io_ctx()).remove(_oid) < 0) {
    i_debug("Librados obj: %s , could not be removed", _oid.c_str());
  }
}

// delegate completion call to given rados object
static void rbox_transaction_private_complete_callback(rados_completion_t comp, void *arg) {
  RadosMailObject *rados_mail_object = reinterpret_cast<RadosMailObject *>(arg);
  i_debug("mail_saved ! %s callback ", rados_mail_object->get_oid().c_str());
}

void clean_up_failed(struct rbox_save_context *_r_ctx) {
  struct rbox_storage *r_storage = (struct rbox_storage *)&_r_ctx->mbox->storage->storage;

  // do some expunges
  mail_index_expunge(_r_ctx->trans, _r_ctx->seq);
  int ret = r_storage->s->get_io_ctx().aio_flush_async(_r_ctx->current_object->get_completion_private().get());
  if (ret > 0) {
    if (_r_ctx->current_object->get_completion_private()->get_return_value() >= 0) {
      mail_cache_transaction_reset(_r_ctx->ctx.transaction->cache_trans);
      _r_ctx->current_object->get_write_op().remove();
      remove_from_rados(r_storage->s, _r_ctx->current_object->get_oid());
    }
  }
  _r_ctx->mail_count--;
}

void clean_up_write_finish(struct mail_save_context *_ctx) {
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  if (r_ctx->copying != TRUE) {
    index_mail_cache_parse_deinit(_ctx->dest_mail, _ctx->data.received_date, !r_ctx->failed);
    if (r_ctx->input != NULL) {
      i_stream_unref(&r_ctx->input);
    }
  }
  index_save_context_free(_ctx);
}

int rbox_save_finish(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;
  int ret = -1;

  r_ctx->finished = TRUE;
  if (_ctx->data.save_date != (time_t)-1) {
    struct index_mail *mail = (struct index_mail *)_ctx->dest_mail;
    uint32_t t = _ctx->data.save_date;
    index_mail_cache_add(mail, MAIL_CACHE_SAVE_DATE, &t, sizeof(t));
  }

  if (!r_ctx->failed) {
    ret = r_ctx->current_object->get_completion_private()->set_complete_callback(
        r_ctx->current_object, rbox_transaction_private_complete_callback);

    if (ret == 0) {
      if (r_ctx->copying != TRUE) {
        i_debug("copying is true ");
        rbox_save_mail_write_metadata(r_ctx);
        librados::bufferlist mail_data_bl;
        mail_data_bl.append(str_c(r_ctx->mail_buffer));
        r_ctx->current_object->get_write_op().write_full(mail_data_bl);

        // MAKE SYNC, ASYNC
        ret = r_storage->s->get_io_ctx().aio_operate(r_ctx->current_object->get_oid(),
                                                     r_ctx->current_object->get_completion_private().get(),
                                                     &r_ctx->current_object->get_write_op());
      }
    }
    r_ctx->failed = ret < 0;
  }

  if (r_ctx->failed) {
    clean_up_failed(r_ctx);
  } else {
    r_ctx->mail_count++;
  }

  clean_up_write_finish(_ctx);
  debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_finish", NULL);

  FUNC_END();
  return r_ctx->failed ? -1 : 0;
}

void rbox_save_cancel(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  r_ctx->failed = TRUE;
  (void)rbox_save_finish(_ctx);

  debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_cancel", NULL);
  FUNC_END();
}

int rbox_transaction_save_commit_pre(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct mailbox_transaction_context *_t = _ctx->transaction;
  const struct mail_index_header *hdr;
  struct seq_range_iter iter;
  uint32_t uid;
  const char *dir;
  string_t *src_path, *dest_path;
  unsigned int n;
  size_t src_prefixlen, dest_prefixlen;

  i_assert(r_ctx->finished);

  if (rbox_sync_begin(r_ctx->mbox, &r_ctx->sync_ctx, TRUE) < 0) {
    r_ctx->failed = TRUE;
    rbox_transaction_save_rollback(_ctx);
    debug_print_mail_save_context(_ctx, "rbox-save::rbox_transaction_save_commit_pre (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  hdr = mail_index_get_header(r_ctx->sync_ctx->sync_view);
  mail_index_append_finish_uids(r_ctx->trans, hdr->next_uid, &_t->changes->saved_uids);
  _t->changes->uid_validity = r_ctx->sync_ctx->uid_validity;

  debug_print_mail_save_context(_ctx, "rbox-save::rbox_transaction_save_commit_pre", NULL);
  FUNC_END();
  return 0;
}

void rbox_transaction_save_commit_post(struct mail_save_context *_ctx,
                                       struct mail_index_transaction_commit_result *result) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  if (r_ctx->copying != TRUE) {
    int ret = r_storage->s->get_io_ctx().aio_flush_async(r_ctx->current_object->get_completion_private().get());
    if (ret != 0) {
      r_ctx->failed = true;
    } else if (r_ctx->current_object->get_completion_private()->get_return_value() < 0) {
      r_ctx->failed = true;
    }

    // clean
    r_ctx->current_object->get_write_op().remove();
    if (r_ctx->failed) {
      clean_up_failed(r_ctx);
    }
  }

  _ctx->transaction = NULL; /* transaction is already freed */

  mail_index_sync_set_commit_result(r_ctx->sync_ctx->index_sync_ctx, result);

  (void)rbox_sync_finish(&r_ctx->sync_ctx, TRUE);
  debug_print_mail_save_context(_ctx, "rbox-save::rbox_transaction_save_commit_post", NULL);

  // TODO(jrse) create cleanup function
  rbox_transaction_save_rollback(_ctx);

  FUNC_END();
}

void rbox_transaction_save_rollback(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;

  if (!r_ctx->finished) {
    rbox_save_cancel(&r_ctx->ctx);
  }

  if (r_ctx->sync_ctx != NULL)
    (void)rbox_sync_finish(&r_ctx->sync_ctx, FALSE);

  if (r_ctx->failed) {
    remove_from_rados(r_storage->s, r_ctx->current_object->get_oid());
  }

  debug_print_mail_save_context(_ctx, "rbox-save::rbox_transaction_save_rollback", NULL);

  buffer_free(&r_ctx->mail_buffer);
  delete r_ctx->current_object;
  delete r_ctx;
  FUNC_END();
}
