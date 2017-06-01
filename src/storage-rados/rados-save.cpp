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
  explicit rados_save_context(RadosStorage &rados_storage) : mbox(NULL), trans(NULL),
                                                             mail_count(0), sync_ctx(NULL),
                                                             seq(0), input(NULL),
                                                             rados_storage(rados_storage),
                                                             mailObject(NULL), failed(1),
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
  ObjectWriteOperation write_op;
  ObjectWriteOperation rollback_op;

  string cur_oid;
  std::vector<std::string> saved_oids;

  RadosMailObject *mailObject;

  unsigned int failed : 1;
  unsigned int finished : 1;
};

struct mail_save_context *rados_save_alloc(struct mailbox_transaction_context *t) {
  FUNC_START();
  struct rados_mailbox *mbox = (struct rados_mailbox *)t->box;
  struct rados_storage *storage = (struct rados_storage *)mbox->storage;
  rados_save_context *ctx;

  i_assert((t->flags & MAILBOX_TRANSACTION_FLAG_EXTERNAL) != 0);

  if (t->save_ctx == NULL) {
    ctx = new rados_save_context(*(storage->s));
    ctx->ctx.transaction = t;
    ctx->mbox = mbox;
    ctx->trans = t->itrans;
    t->save_ctx = &ctx->ctx;
  }
  debug_print_mail_save_context(t->save_ctx, "rados-save::rados_save_alloc", NULL);

  ctx->mailObject = new RadosMailObject();

  FUNC_END();
  return t->save_ctx;
}

int rados_save_begin(struct mail_save_context *_ctx, struct istream *input) {
  FUNC_START();
  rados_save_context *ctx = (struct rados_save_context *)_ctx;
  struct mailbox_transaction_context *trans = _ctx->transaction;
  int save_flags;
  struct istream *crlf_input;

  ctx->failed = FALSE;

  guid_128_generate(ctx->mail_oid);

  string oid(guid_128_to_string(ctx->mail_oid));
  ctx->cur_oid = oid;
  i_debug("rados_save_begin: saving to %s", ctx->cur_oid.c_str());

  if (ctx->failed) {
    debug_print_mail_save_context(_ctx, "rados-save::rados_save_begin (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  /* add to index */
  save_flags = _ctx->data.flags & ~MAIL_RECENT;
  mail_index_append(ctx->trans, 0, &ctx->seq);
  mail_index_update_flags(ctx->trans, ctx->seq, MODIFY_REPLACE, static_cast<mail_flags>(save_flags));
  if (_ctx->data.keywords != NULL) {
    mail_index_update_keywords(ctx->trans, ctx->seq, MODIFY_REPLACE, _ctx->data.keywords);
  }
  if (_ctx->data.min_modseq != 0) {
    mail_index_update_modseq(ctx->trans, ctx->seq, _ctx->data.min_modseq);
  }

  mail_set_seq_saving(_ctx->dest_mail, ctx->seq);

  crlf_input = i_stream_create_crlf(input);
  ctx->input = index_mail_cache_parse_init(_ctx->dest_mail, crlf_input);
  i_stream_unref(&crlf_input);
  debug_print_mail_save_context(_ctx, "rados-save::rados_save_begin", NULL);
  FUNC_END();
  return ctx->failed ? -1 : 0;
}

size_t rados_stream_mail_to_rados(istream *input, struct rados_save_context *ctx) {
  const unsigned char *data;
  ssize_t ret;
  ceph::bufferlist bl;
  size_t size;
  size_t total_size = 0;

  do {
    (void)i_stream_read_data(input, &data, &size, 0);
    i_debug("rados_stream_mail_to_rados: size=%lu", size);
    if (size == 0) {
      /*all sent */
      if (input->stream_errno != 0) {
        return -1;
      }
      break;
    }

    bl.clear();
    bl.append((const char *)data, size);

    ceph::bufferlist cmp_bl;
    cmp_bl.append("S");
    ctx->write_op.write(total_size, bl);

    total_size += bl.length();
    i_stream_skip(input, bl.length());
  } while ((size_t)ret == size);

  return total_size;
}

int rados_save_continue(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *ctx = (struct rados_save_context *)_ctx;
  struct mail_storage *storage = &ctx->mbox->storage->storage;

  if (ctx->failed) {
    debug_print_mail_save_context(_ctx, "rados-save::rados_save_continue (ret -1, 1)", NULL);
    debug_print_mail_storage(storage, "rados-save::rados_save_continue (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  do {
    if (rados_stream_mail_to_rados(ctx->input, ctx) < 0) {
      //    if (rados_stream_mail_to_rados(ctx) < 0) {
      if (!mail_storage_set_error_from_errno(storage)) {
        mail_storage_set_critical(storage, "write(%s) failed: %m", "unused JAR");
      }
      ctx->failed = TRUE;
      debug_print_mail_save_context(_ctx, "rados-save::rados_save_continue (ret -1, 2)", NULL);
      debug_print_mail_storage(storage, "rados-save::rados_save_continue (ret -1, 2)", NULL);
      FUNC_END_RET("ret == -1");
      return -1;
    }
    index_mail_cache_parse_continue(_ctx->dest_mail);

    /* both tee input readers may consume data from our primary
     input stream. we'll have to make sure we don't return with
     one of the streams still having data in them. */
  } while (i_stream_read(ctx->input) > 0);

  debug_print_mail_save_context(_ctx, "rados-save::rados_save_continue", NULL);
  debug_print_mail_storage(storage, "rados-save::rados_save_continue", NULL);
  FUNC_END();
  return 0;
}

static int rados_save_mail_write_metadata(struct rados_save_context *ctx) {
  FUNC_START();
  struct rados_mailbox *rbox = ctx->mbox;
  struct mail_storage *storage = ctx->ctx.transaction->box->storage;
  auto *rados_storage = (struct rados_storage *)storage;
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
  mail_index_update_ext(ctx->trans, ctx->seq, rbox->ext_id, &rec, NULL);

  {
    bufferlist bl;
    bl.append((const char *)ctx->mail_guid, sizeof(ctx->mail_guid));
    ctx->write_op.setxattr(RBOX_METADATA_GUID, bl);
  }

  {
    bufferlist bl;
    bl.append(mdata->save_date);
    ctx->write_op.setxattr(RBOX_METADATA_SAVE_DATE, bl);
  }

  if (mdata->pop3_uidl != NULL) {
    i_assert(strchr(mdata->pop3_uidl, '\n') == NULL);
    bufferlist bl;
    bl.append(mdata->pop3_uidl);
    ctx->write_op.setxattr(RBOX_METADATA_POP3_UIDL, bl);
  }

  if (mdata->pop3_order != 0) {
    bufferlist bl;
    bl.append(mdata->pop3_order);
    ctx->write_op.setxattr(RBOX_METADATA_POP3_ORDER, bl);
  }

  {
    bufferlist bl;
    bl.append(mdata->received_date);
    ctx->write_op.setxattr(RBOX_METADATA_RECEIVED_DATE, bl);
    ctx->write_op.mtime(&mdata->received_date);
  }

  FUNC_END();

  return 0;
}

int rados_save_finish(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *ctx = (struct rados_save_context *)_ctx;
  struct rados_mailbox *rbox = ctx->mbox;
  struct mail_storage *storage = &ctx->mbox->storage->storage;
  struct rados_storage *rados_ctx = (struct rados_storage *)storage;

  ctx->finished = TRUE;

  if (!ctx->failed) {
    rados_save_mail_write_metadata(ctx);

    i_debug("saving to : %s", ctx->cur_oid.c_str());
    int ret = rados_ctx->s->get_io_ctx().operate(ctx->cur_oid, &ctx->write_op);

    if (ret < 0) {
      i_debug("ERRO saving object to rados");
      i_debug("rados_save_finish(): saving object %s to rados failed err=%d(%s)", ctx->cur_oid.c_str(), ret,
              strerror(-ret));
      ctx->failed = TRUE;
    }
  }

  if (!ctx->failed) {
    ctx->mail_count++;
    ctx->saved_oids.push_back(ctx->cur_oid);
  } else {
    ctx->write_op.remove();
  }

  index_mail_cache_parse_deinit(_ctx->dest_mail, _ctx->data.received_date, !ctx->failed);

  if (ctx->input != NULL)
    i_stream_unref(&ctx->input);

  index_save_context_free(_ctx);
  debug_print_mail_save_context(_ctx, "rados-save::rados_save_finish", NULL);
  FUNC_END();
  return ctx->failed ? -1 : 0;
}

void rados_save_cancel(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *ctx = (struct rados_save_context *)_ctx;

  ctx->failed = TRUE;
  (void)rados_save_finish(_ctx);

  debug_print_mail_save_context(_ctx, "rados-save::rados_save_cancel", NULL);
  FUNC_END();
}

int rados_transaction_save_commit_pre(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *ctx = (struct rados_save_context *)_ctx;
  struct mailbox_transaction_context *_t = _ctx->transaction;
  const struct mail_index_header *hdr;
  struct seq_range_iter iter;
  uint32_t uid;
  const char *dir;
  string_t *src_path, *dest_path;
  unsigned int n;
  size_t src_prefixlen, dest_prefixlen;

  i_assert(ctx->finished);

  if (rados_sync_begin(ctx->mbox, &ctx->sync_ctx, TRUE) < 0) {
    ctx->failed = TRUE;
    rados_transaction_save_rollback(_ctx);
    debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_commit_pre (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  hdr = mail_index_get_header(ctx->sync_ctx->sync_view);
  mail_index_append_finish_uids(ctx->trans, hdr->next_uid, &_t->changes->saved_uids);
  _t->changes->uid_validity = ctx->sync_ctx->uid_validity;

  debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_commit_pre", NULL);
  FUNC_END();
  return 0;
}

void rados_transaction_save_commit_post(struct mail_save_context *_ctx,
                                        struct mail_index_transaction_commit_result *result) {
  FUNC_START();
  struct rados_save_context *ctx = (struct rados_save_context *)_ctx;

  _ctx->transaction = NULL; /* transaction is already freed */

  mail_index_sync_set_commit_result(ctx->sync_ctx->index_sync_ctx, result);

  (void)rados_sync_finish(&ctx->sync_ctx, TRUE);
  debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_commit_post", NULL);
  rados_transaction_save_rollback(_ctx);
  FUNC_END();
}

void rados_transaction_save_rollback(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rados_save_context *ctx = (struct rados_save_context *)_ctx;

  if (!ctx->finished)
    rados_save_cancel(&ctx->ctx);

  if (ctx->sync_ctx != NULL)
    (void)rados_sync_finish(&ctx->sync_ctx, FALSE);

  debug_print_mail_save_context(_ctx, "rados-save::rados_transaction_save_rollback", NULL);

  delete ctx->mailObject;
  delete ctx;
  FUNC_END();
}
