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
#include "time.h"
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
    r_ctx->current_object = nullptr;
    t->save_ctx = &r_ctx->ctx;
  }
  debug_print_mail_save_context(t->save_ctx, "rbox-save::rbox_save_alloc", NULL);

  FUNC_END();
  return t->save_ctx;
}

void rbox_add_to_index(struct mail_save_context *_ctx) {
  struct mail_save_data *mdata = &_ctx->data;
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  int save_flags;

  /* add to index */
  save_flags = mdata->flags & ~MAIL_RECENT;
  mail_index_append(r_ctx->trans, 0, &r_ctx->seq);
  i_debug("add seq %d to index ", r_ctx->seq);

  mail_index_update_flags(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE, static_cast<mail_flags>(save_flags));
  i_debug("update flags for seq %d ", r_ctx->seq);

  if (_ctx->data.keywords != NULL) {
    mail_index_update_keywords(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE, _ctx->data.keywords);
  }
  if (_ctx->data.min_modseq != 0) {
    mail_index_update_modseq(r_ctx->trans, r_ctx->seq, _ctx->data.min_modseq);
  }

  guid_128_generate(r_ctx->mail_oid);

  // save old ptr.
  if (r_ctx->current_object != nullptr) {
    r_ctx->objects.push_back(r_ctx->current_object);
  }

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
  mail_index_update_ext(r_ctx->trans, r_ctx->seq, r_ctx->mbox->ext_id, &rec, NULL);

  i_debug("SAVE OID: %s, %d uid , seq=%d", guid_128_to_string(rec.oid), _ctx->dest_mail->uid, r_ctx->seq);

  mail_set_seq_saving(_ctx->dest_mail, r_ctx->seq);
}

int rbox_save_begin(struct mail_save_context *_ctx, struct istream *input) {
  FUNC_START();
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
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

  debug_print_mail_save_context(_ctx, "rbox-save::rbox_save_begin", NULL);

  FUNC_END();
  return 0;
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
        mail_storage_set_critical(storage, "write(%s) failed: %m", o_stream_get_name(_ctx->data.output));
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

int rbox_save_mail_write_metadata(struct rbox_save_context *ctx) {
  FUNC_START();
  struct mail_save_data *mdata = &ctx->ctx.data;

  {
    bufferlist bl;
    bl.append(guid_128_to_string(ctx->mail_guid));
    ctx->current_object->get_write_op().setxattr(RadosMailObject::X_ATTR_GUID.c_str(), bl);
  }
  {
    bufferlist bl;
    long ts = static_cast<long int>(mdata->save_date);
    bl.append(std::to_string(ts));
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
    bl.append(std::to_string(mdata->pop3_order));
    ctx->current_object->get_write_op().setxattr(RadosMailObject::X_ATTR_POP3_ORDER.c_str(), bl);
  }

  ctx->current_object->get_write_op().mtime(&mdata->received_date);

  FUNC_END();
  return 0;
}

void remove_from_rados(librmb::RadosStorage *_storage, const std::string &_oid) {
  if ((_storage->get_io_ctx()).remove(_oid) < 0) {
    i_debug("Librados obj: %s , could not be removed", _oid.c_str());
  }
  i_debug("removed oid=%s", _oid.c_str());
}


void clean_up_failed(struct rbox_save_context *r_ctx) {
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  int ret = r_ctx->current_object->get_completion_private()->wait_for_safe_and_cb();

  if (ret >= 0) {
    if (r_ctx->current_object->get_completion_private()->get_return_value() >= 0) {
      r_ctx->current_object->get_write_op().remove();
      remove_from_rados(r_storage->s, r_ctx->current_object->get_oid());
    }
  }
  // clean up index
  mail_index_expunge(r_ctx->trans, r_ctx->seq);
  mail_cache_transaction_reset(r_ctx->ctx.transaction->cache_trans);
  r_ctx->current_object->get_completion_private()->release();
  r_ctx->mail_count--;
}

void clean_up_write_finish(struct mail_save_context *_ctx) {
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  if (r_ctx->copying != TRUE) {
    index_mail_cache_parse_deinit(_ctx->dest_mail, _ctx->data.received_date, !r_ctx->failed);
  }
  if (r_ctx->input != NULL) {
    i_stream_unref(&r_ctx->input);
  }
  index_save_context_free(_ctx);
}

void split_buffer_operation(const buffer_t *buffer, size_t buffer_length, uint64_t max_size,
                            struct rbox_save_context *r_ctx) {
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;
  size_t write_buffer_size = buffer_length;

  assert(max_size > 0);

  int rest = write_buffer_size % max_size;
  // TODO(jrse) move op_lit to rados_object
  std::vector<AioCompletion *> op_list;

  int div = write_buffer_size / max_size + (rest > 0 ? 1 : 0);
  for (int i = 0; i < div; i++) {
    int offset = i * max_size;
    librados::ObjectWriteOperation op;

    int length = max_size;
    if (buffer_length < ((i + 1) * length)) {
      length = rest;
    }
    const char *buf = (char *)buffer->data + offset;
    librados::bufferlist tmp;
    tmp.append(buf, length);
    op.write(offset, tmp);

    AioCompletion *completion = librados::Rados::aio_create_completion();

    i_debug("creation aio operation %s:", r_ctx->current_object->get_oid().c_str());
    // MAKE SYNC, ASYNC
    r_storage->s->get_io_ctx().aio_operate(r_ctx->current_object->get_oid(), completion, &op);
    op_list.push_back(completion);
  }

  // Iterate and print values of vector
  for (AioCompletion *n : op_list) {
    n->wait_for_safe_and_cb();
    n->release();
  }
}

int rbox_save_finish(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;
  int ret = 0;

  r_ctx->finished = TRUE;
  if (_ctx->data.save_date != (time_t)-1) {
    struct index_mail *mail = (struct index_mail *)_ctx->dest_mail;

    uint32_t t = _ctx->data.save_date;
    index_mail_cache_add(mail, MAIL_CACHE_SAVE_DATE, &t, sizeof(t));
  } else {
    _ctx->data.save_date = time(NULL);
  }

  if (!r_ctx->failed) {
    if (ret == 0) {
      if (r_ctx->copying != TRUE) {
        ret = r_ctx->current_object->get_completion_private()->set_complete_callback(r_ctx->current_object, nullptr);
        rbox_save_mail_write_metadata(r_ctx);

        size_t write_buffer_size = buffer_get_size(r_ctx->mail_buffer);

        int max_write_size = r_storage->s->get_max_write_size_bytes();
        i_debug("OSD_MAX_WRITE_SIZE=%dmb", (max_write_size / 1024 / 1024));

        if (write_buffer_size > max_write_size) {
          i_debug("file to big %lu buffer , max %d ", (unsigned long)write_buffer_size,
                  r_storage->s->get_max_write_size_bytes());
          split_buffer_operation(r_ctx->mail_buffer, write_buffer_size, max_write_size, r_ctx);

        } else {
          librados::bufferlist mail_data_bl;
          mail_data_bl.append(str_c(r_ctx->mail_buffer));
          r_ctx->current_object->get_write_op().write_full(mail_data_bl);
        }
        // MAKE SYNC, ASYNC
        ret = r_storage->s->get_io_ctx().aio_operate(r_ctx->current_object->get_oid(),
                                                     r_ctx->current_object->get_completion_private(),
                                                     &r_ctx->current_object->get_write_op());
        // set current_objects operation to active!
        r_ctx->current_object->set_active_op(true);
        i_debug("async operate executed oid: %s , ret=%d", r_ctx->current_object->get_oid().c_str(), ret);
      }
    }
    r_ctx->failed = ret < 0;
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
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  i_assert(r_ctx->finished);

  r_ctx->objects.push_back(r_ctx->current_object);

  // if we come from copy mail, there is no operation to wait for.
  if (r_ctx->current_object->has_active_op()) {
    // wait for all writes to finish!
    // imaptest shows it's possible that begin -> continue -> finish cycle is invoked several times before
    // rbox_transaction_save_commit_pre is called.
    for (std::vector<librmb::RadosMailObject *>::iterator it = r_ctx->objects.begin(); it != r_ctx->objects.end();
         ++it) {
      r_ctx->current_object = *it;
      // note: wait_for_complete_and_cb will also wait if there is no active op.
      int ret = r_ctx->current_object->get_completion_private()->wait_for_complete_and_cb();
      if (ret != 0) {
        r_ctx->failed = true;
      } else if (r_ctx->current_object->get_completion_private()->get_return_value() < 0) {
        r_ctx->failed = true;
      }

      i_debug("OID %s , SAVED success=%s", r_ctx->current_object->get_oid().c_str(),
              r_ctx->failed ? "false" : "true");  //, file_size);

      if (r_ctx->failed) {
        break;
      }
    }
    // if one write fails! all writes will be reverted and r_ctx->failed is true!
    if (r_ctx->failed) {
      for (std::vector<librmb::RadosMailObject *>::iterator it = r_ctx->objects.begin(); it != r_ctx->objects.end();
           ++it) {
        r_ctx->current_object = *it;
        // delete index entry and delete object if it exist
        // remove entry from index is not successful in rbox_transaction_commit_post
        // clean up will wait for object operation to complete
        clean_up_failed(r_ctx);
      }
    }
  }

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

  _ctx->transaction = NULL; /* transaction is already freed */

  mail_index_sync_set_commit_result(r_ctx->sync_ctx->index_sync_ctx, result);

  (void)rbox_sync_finish(&r_ctx->sync_ctx, TRUE);
  debug_print_mail_save_context(_ctx, "rbox-save::rbox_transaction_save_commit_post", NULL);

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
    // delete index entry and delete object if it exist
    clean_up_failed(r_ctx);
  }

  debug_print_mail_save_context(_ctx, "rbox-save::rbox_transaction_save_rollback", NULL);

  buffer_free(&r_ctx->mail_buffer);
  for (std::vector<librmb::RadosMailObject *>::iterator it = r_ctx->objects.begin(); it != r_ctx->objects.end(); ++it) {
    delete *it;
  }
  r_ctx->objects.clear();

  guid_128_empty(r_ctx->mail_guid);
  guid_128_empty(r_ctx->mail_oid);

  delete r_ctx;

  FUNC_END();
}
