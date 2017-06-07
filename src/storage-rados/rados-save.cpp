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
#include "rados-storage-local.h"
#include "rados-sync.h"
#include "debug-helper.h"
}

#include "../librmb/rados-mail-object.h"
#include "rados-storage-struct.h"
#include "rados-storage.h"

using namespace librados;  // NOLINT
using namespace librmb;    // NOLINT

using std::string;

class rados_save_context {
 public:
  explicit rados_save_context(RadosStorage &rados_storage)
      : mbox(NULL),
        trans(NULL),
        mail_count(0),
        sync_ctx(NULL),
        seq(0),
        input(NULL),
        rados_storage(rados_storage),
        mail_object(NULL),
        failed(1),
        finished(1) {}

  struct mail_save_context ctx;

  struct rados_mailbox *mbox;
  struct mail_index_transaction *trans;

  unsigned int mail_count;

  guid_128_t mail_guid;  // goes to index record
  guid_128_t mail_oid;   // goes to index record

  struct rados_sync_context *sync_ctx;

  /* updated for each appended mail: */
  uint32_t seq;
  struct istream *input;

  RadosStorage &rados_storage;

  string cur_oid;
  std::vector<std::string> saved_oids;

  RadosMailObject *mail_object;

  unsigned int failed : 1;
  unsigned int finished : 1;
};

struct mail_save_context *rados_save_alloc(struct mailbox_transaction_context *t) {
  FUNC_START();
  struct rados_mailbox *mbox = (struct rados_mailbox *)t->box;
  struct rados_storage *r_storage = (struct rados_storage *)mbox->storage;
  rados_save_context *r_ctx;

  i_assert((t->flags & MAILBOX_TRANSACTION_FLAG_EXTERNAL) != 0);

  if (t->save_ctx == NULL) {
    i_debug("rados_save_alloc: t->save_ctx == NULL, mailbox name = %s", t->box->name);
    r_ctx = new rados_save_context(*(r_storage->s));
    r_ctx->ctx.transaction = t;
    r_ctx->mbox = mbox;
    r_ctx->trans = t->itrans;
    t->save_ctx = &r_ctx->ctx;
  }
  debug_print_mail_save_context(t->save_ctx, "rados-save::rados_save_alloc", NULL);


  FUNC_END();
  return t->save_ctx;
}

int rados_save_begin(struct mail_save_context *_ctx, struct istream *input) {
  FUNC_START();
  rados_save_context *r_ctx = (struct rados_save_context *)_ctx;
  i_debug(" rados_save_begin");

  struct mailbox_transaction_context *trans = _ctx->transaction;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;
  struct rados_storage *r_storage = (struct rados_storage *)storage;

  int save_flags;
  struct istream *crlf_input;

  r_ctx->failed = FALSE;

  guid_128_generate(r_ctx->mail_oid);

  r_ctx->mail_object = new RadosMailObject();

  r_ctx->mail_object->set_oid(guid_128_to_string(r_ctx->mail_oid));

  bufferlist version_bl;
  version_bl.append(RadosMailObject::X_ATTR_VERSION_VALUE);

  r_ctx->mail_object->get_write_op().setxattr(RadosMailObject::X_ATTR_VERSION.c_str(), version_bl);
  if (r_ctx->failed) {
    debug_print_mail_save_context(_ctx, "rados-save::rados_save_begin (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  /* add to index */
  save_flags = _ctx->data.flags & ~MAIL_RECENT;
  mail_index_append(r_ctx->trans, 0, &r_ctx->seq);
  mail_index_update_flags(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE, static_cast<mail_flags>(save_flags));
  if (_ctx->data.keywords != NULL) {
    mail_index_update_keywords(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE, _ctx->data.keywords);
  }
  if (_ctx->data.min_modseq != 0) {
    mail_index_update_modseq(r_ctx->trans, r_ctx->seq, _ctx->data.min_modseq);
  }

  mail_set_seq_saving(_ctx->dest_mail, r_ctx->seq);

  crlf_input = i_stream_create_crlf(input);
  r_ctx->input = index_mail_cache_parse_init(_ctx->dest_mail, crlf_input);
  i_stream_unref(&crlf_input);
  debug_print_mail_save_context(_ctx, "rados-save::rados_save_begin", NULL);
  FUNC_END();
  return r_ctx->failed ? -1 : 0;
}

size_t rados_stream_mail_to_buffer(istream *_input, struct rados_save_context *_ctx) {
  const unsigned char *data;
  size_t size = 0;
  struct mail_storage *storage = &_ctx->mbox->storage->storage;
  struct rados_storage *r_storage = (struct rados_storage *)storage;

  do {
    (void *)i_stream_read_data(_input, &data, &size, 0);
    if (size == 0) {
      /*all sent */
      if (_input->stream_errno != 0) {
        return -1;
      }
      break;
    }
    _ctx->mail_object->get_mail_data_ref().append((const char *)data, size);
    i_stream_skip(_input, size);
  } while (size > 0);

  return _ctx->mail_object->get_bytes_written();
}

int rados_save_continue(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *r_ctx = (struct rados_save_context *)_ctx;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;

  if (r_ctx->failed) {
    debug_print_mail_save_context(_ctx, "rados-save::rados_save_continue (ret -1, 1)", NULL);
    debug_print_mail_storage(storage, "rados-save::rados_save_continue (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  do {
    if (rados_stream_mail_to_buffer(r_ctx->input, r_ctx) < 0) {
      if (!mail_storage_set_error_from_errno(storage)) {
        mail_storage_set_critical(storage, "write(%s) failed: %m", "unused JAR");
      }
      r_ctx->failed = TRUE;
      debug_print_mail_save_context(_ctx, "rados-save::rados_save_continue (ret -1, 2)", NULL);
      debug_print_mail_storage(storage, "rados-save::rados_save_continue (ret -1, 2)", NULL);
      FUNC_END_RET("ret == -1");
      return -1;
    }
    index_mail_cache_parse_continue(_ctx->dest_mail);

    /* both tee input readers may consume data from our primary
     input stream. we'll have to make sure we don't return with
     one of the streams still having data in them. */
  } while (i_stream_read(r_ctx->input) > 0);

  debug_print_mail_save_context(_ctx, "rados-save::rados_save_continue", NULL);
  debug_print_mail_storage(storage, "rados-save::rados_save_continue", NULL);
  FUNC_END();
  return 0;
}

static int rados_save_mail_write_metadata(struct rados_save_context *ctx) {
  FUNC_START();
  struct rados_mailbox *mbox = ctx->mbox;
  struct mail_storage *storage = ctx->ctx.transaction->box->storage;
  struct rados_storage *r_storage = (struct rados_storage *)storage;
  struct mail_save_data *mdata = &ctx->ctx.data;

  if (ctx->ctx.data.guid != NULL) {
    mail_generate_guid_128_hash(ctx->ctx.data.guid, ctx->mail_guid);
  } else {
    guid_128_generate(ctx->mail_guid);
  }

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  i_zero(&rec);
  memcpy(rec.guid, ctx->mail_guid, sizeof(ctx->mail_guid));
  memcpy(rec.oid, ctx->mail_oid, sizeof(ctx->mail_oid));
  mail_index_update_ext(ctx->trans, ctx->seq, mbox->ext_id, &rec, NULL);

  {
    bufferlist bl;
    bl.append((const char *)ctx->mail_guid, sizeof(ctx->mail_guid));
    ctx->mail_object->get_write_op().setxattr(RadosMailObject::X_ATTR_GUID.c_str(), bl);
  }

  {
    bufferlist bl;
    bl.append(mdata->save_date);
    ctx->mail_object->get_write_op().setxattr(RadosMailObject::X_ATTR_SAVE_DATE.c_str(), bl);
  }

  if (mdata->pop3_uidl != NULL) {
    i_assert(strchr(mdata->pop3_uidl, '\n') == NULL);
    bufferlist bl;
    bl.append(mdata->pop3_uidl);
    ctx->mail_object->get_write_op().setxattr(RadosMailObject::X_ATTR_POP3_UIDL.c_str(), bl);
  }

  if (mdata->pop3_order != 0) {
    bufferlist bl;
    bl.append(mdata->pop3_order);
    ctx->mail_object->get_write_op().setxattr(RadosMailObject::X_ATTR_POP3_ORDER.c_str(), bl);
  }

  { ctx->mail_object->get_write_op().mtime(&mdata->received_date); }

  FUNC_END();
  return 0;
}
static void remove_from_rados(librmb::RadosStorage *_storage, const std::string &_oid) {
  i_debug("object to delete oid is: %s", _oid);
  if ((_storage->get_io_ctx()).remove(_oid) < 0) {
    i_debug("Librados obj: %s , could not be removed", _oid);
  }
}
int rados_save_finish(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *r_ctx = (struct rados_save_context *)_ctx;
  struct rados_mailbox *mbox = r_ctx->mbox;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;
  struct rados_storage *r_storage = (struct rados_storage *)storage;

  if (!r_ctx->failed) {
    rados_save_mail_write_metadata(r_ctx);

    librados::bufferlist mail_data_bl;
    mail_data_bl.append(r_ctx->mail_object->get_mail_data_ref());
    r_ctx->mail_object->get_write_op().write_full(mail_data_bl);

    int ret = r_storage->s->get_io_ctx().operate(r_ctx->mail_object->get_oid(), &r_ctx->mail_object->get_write_op());
    i_debug("saving to : %s", r_ctx->mail_object->get_oid().c_str());
    if (ret < 0) {
      i_debug("ERROR saving object to rados");
      i_debug("rados_save_finish(): saving object %s to rados failed err=%d(%s)", r_ctx->mail_object->get_oid().c_str(), ret,
              strerror(-ret));
      r_ctx->failed = TRUE;
    }
  }

  if (!r_ctx->failed) {
    r_ctx->mail_count++;
    r_ctx->saved_oids.push_back(r_ctx->mail_object->get_oid());
  } else {
    // revert index attribute also

    r_ctx->mail_object->get_write_op().remove();
    remove_from_rados(r_storage->s, r_ctx->mail_object->get_oid());
  }

  r_ctx->finished = TRUE;

  index_mail_cache_parse_deinit(_ctx->dest_mail, _ctx->data.received_date, !r_ctx->failed);
  if (r_ctx->input != NULL)
    i_stream_unref(&r_ctx->input);

  index_save_context_free(_ctx);
  debug_print_mail_save_context(_ctx, "rados-save::rados_save_finish", NULL);
  FUNC_END();
  return r_ctx->failed ? -1 : 0;
}

void rados_save_cancel(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *r_ctx = (struct rados_save_context *)_ctx;

  r_ctx->failed = TRUE;
  (void)rados_save_finish(_ctx);

  // delete save_oids.

  debug_print_mail_save_context(_ctx, "rados-save::rados_save_cancel", NULL);
  FUNC_END();
}

int rados_transaction_save_commit_pre(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *r_ctx = (struct rados_save_context *)_ctx;
  struct mailbox_transaction_context *_t = _ctx->transaction;
  const struct mail_index_header *hdr;
  struct seq_range_iter iter;
  uint32_t uid;
  const char *dir;
  string_t *src_path, *dest_path;
  unsigned int n;
  size_t src_prefixlen, dest_prefixlen;

  i_assert(r_ctx->finished);

  if (rados_sync_begin(r_ctx->mbox, &r_ctx->sync_ctx, TRUE) < 0) {
    r_ctx->failed = TRUE;
    rados_transaction_save_rollback(_ctx);
    debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_commit_pre (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  hdr = mail_index_get_header(r_ctx->sync_ctx->sync_view);
  mail_index_append_finish_uids(r_ctx->trans, hdr->next_uid, &_t->changes->saved_uids);
  _t->changes->uid_validity = r_ctx->sync_ctx->uid_validity;

  debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_commit_pre", NULL);
  FUNC_END();
  return 0;
}

void rados_transaction_save_commit_post(struct mail_save_context *_ctx,
                                        struct mail_index_transaction_commit_result *result) {
  FUNC_START();
  struct rados_save_context *r_ctx = (struct rados_save_context *)_ctx;

  _ctx->transaction = NULL; /* transaction is already freed */

  mail_index_sync_set_commit_result(r_ctx->sync_ctx->index_sync_ctx, result);

  (void)rados_sync_finish(&r_ctx->sync_ctx, TRUE);
  debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_commit_post", NULL);
  rados_transaction_save_rollback(_ctx);
  FUNC_END();
}

void rados_transaction_save_rollback(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *r_ctx = (struct rados_save_context *)_ctx;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;
  struct rados_storage *r_storage = (struct rados_storage *)storage;
  if (!r_ctx->finished) {
    rados_save_cancel(&r_ctx->ctx);
  }

  if (r_ctx->sync_ctx != NULL)
    (void)rados_sync_finish(&r_ctx->sync_ctx, FALSE);

  if (r_ctx->failed) {
    remove_from_rados(r_storage->s, r_ctx->mail_object->get_oid());
  }

  debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_rollback", NULL);

  delete r_ctx->mail_object;
  delete r_ctx;
  FUNC_END();
}
