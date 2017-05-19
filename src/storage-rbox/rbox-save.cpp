/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

extern "C" {

#include "lib.h"
#include "typeof-def.h"
#include "array.h"
#include "fdatasync-path.h"
#include "hex-binary.h"
#include "hex-dec.h"
#include "str.h"
#include "istream.h"
#include "istream-crlf.h"
#include "ostream.h"
#include "write-full.h"
#include "index-mail.h"
#include "mail-copy.h"
#include "index-pop3-uidl.h"
#include "dbox-attachment.h"
#include "dbox-save.h"

#include "rbox-file.h"
#include "rbox-sync.h"
#include "debug-helper.h"

#include "rbox-storage.h"
}
#include "rbox-storage-struct.h"
#include "rados-storage.h"

struct rbox_save_context {
  struct dbox_save_context ctx;

  struct rbox_mailbox *mbox;
  struct rbox_sync_context *sync_ctx;

  struct dbox_file *cur_file;
  struct dbox_file_append_context *append_ctx;

  uint32_t first_saved_seq;
  ARRAY(struct dbox_file *)
  files;
};

struct dbox_file *rbox_save_file_get_file(struct mailbox_transaction_context *t, uint32_t seq) {
  FUNC_START();
  struct rbox_save_context *ctx = (struct rbox_save_context *)t->save_ctx;
  struct dbox_file *const *files, *file;
  unsigned int count;

  i_assert(seq >= ctx->first_saved_seq);

  files = array_get(&ctx->files, &count);
  i_assert(count > 0);
  i_assert(seq - ctx->first_saved_seq < count);

  file = files[seq - ctx->first_saved_seq];
  i_assert(((struct rbox_file *)file)->written_to_disk);

  i_debug("rbox_save_file_get_file: seq = %u", seq);
  rbox_dbg_print_mail_save_context(&ctx->ctx.ctx, "rbox_save_file_get_file", NULL);
  rbox_dbg_print_dbox_file(file, "rbox_save_file_get_file", NULL);
  FUNC_END();
  return file;
}

struct mail_save_context *rbox_save_alloc(struct mailbox_transaction_context *t) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)t->box;
  struct rbox_save_context *ctx = (struct rbox_save_context *)t->save_ctx;

  i_assert((t->flags & MAILBOX_TRANSACTION_FLAG_EXTERNAL) != 0);

  if (ctx != NULL) {
    /* use the existing allocated structure */
    ctx->cur_file = NULL;
    ctx->ctx.failed = FALSE;
    ctx->ctx.finished = FALSE;
    ctx->ctx.dbox_output = NULL;
    rbox_dbg_print_mail_save_context(&ctx->ctx.ctx, "rbox_save_file_get_file", NULL);
    FUNC_END_RET("ret = &ctx->ctx.ctx; use the existing allocated structure");
    return &ctx->ctx.ctx;
  }

  ctx = i_new(struct rbox_save_context, 1);
  ctx->ctx.ctx.transaction = t;
  ctx->ctx.trans = t->itrans;
  ctx->mbox = mbox;
  i_array_init(&ctx->files, 32);
  t->save_ctx = &ctx->ctx.ctx;
  rbox_dbg_print_mail_save_context(&ctx->ctx.ctx, "rbox_save_file_get_file", NULL);
  FUNC_END();
  return t->save_ctx;
}

void rbox_save_add_file(struct mail_save_context *_ctx, struct dbox_file *file) {
  FUNC_START();
  struct rbox_save_context *ctx = (struct rbox_save_context *)_ctx;
  struct dbox_file *const *files;
  unsigned int count;

  if (ctx->first_saved_seq == 0)
    ctx->first_saved_seq = ctx->ctx.seq;

  files = array_get(&ctx->files, &count);
  if (count > 0) {
    /* a plugin may leave a previously saved file open.
     we'll close it here to avoid eating too many fds. */
    dbox_file_close(files[count - 1]);
  }
  array_append(&ctx->files, &file, 1);
  rbox_dbg_print_mail_save_context(_ctx, "rbox_save_add_file", NULL);
  rbox_dbg_print_dbox_file(file, "rbox_save_add_file", NULL);
  FUNC_END();
}

int rbox_save_begin(struct mail_save_context *_ctx, struct istream *input) {
  FUNC_START();
  struct rbox_save_context *ctx = (struct rbox_save_context *)_ctx;
  struct dbox_file *file;
  int ret;

  rbox_dbg_print_mail_save_context(_ctx, "rbox_save_begin", NULL);

  file = rbox_file_create(ctx->mbox);
  ctx->append_ctx = dbox_file_append_init(file);
  ret = dbox_file_get_append_stream(ctx->append_ctx, &ctx->ctx.dbox_output);
  if (ret <= 0) {
    i_assert(ret != 0);
    dbox_file_append_rollback(&ctx->append_ctx);
    dbox_file_unref(&file);
    ctx->ctx.failed = TRUE;
    FUNC_END_RET("ret = -1; get_append_stream failed");
    return -1;
  }
  ctx->cur_file = file;
  dbox_save_begin(&ctx->ctx, input);

  rbox_save_add_file(_ctx, file);
  FUNC_END();
  return ctx->ctx.failed ? -1 : 0;
}

off_t stream_mail_to_rados(const struct rbox_storage *storage, const std::string &oid, struct istream *instream) {
  uoff_t start_offset;
  struct const_iovec iov;
  const unsigned char *data;
  ssize_t ret;
  int offset = 0;
  librados::bufferlist bl;
  start_offset = instream->v_offset;

  do {
    (void)i_stream_read_data(instream, &data, &iov.iov_len, 0);
    if (iov.iov_len == 0) {
      /*all sent */
      if (instream->stream_errno != 0) {
        return -1;
      }
      break;
    }

    iov.iov_base = data;
    bl.clear();
    bl.append((const char *)data);

    int err = ((storage->s)->get_io_ctx()).write(oid, bl, iov.iov_len, offset);
    if (err < 0) {
      return -1;
    }
    i_stream_skip(instream, 0);
  } while ((size_t)ret == iov.iov_len);

  return (off_t)(instream->v_offset - start_offset);
}

int rbox_save_continue(struct mail_save_context *_ctx) {
  FUNC_START();
  struct dbox_save_context *ctx = (struct dbox_save_context *)_ctx;
  struct mail_storage *storage = _ctx->transaction->box->storage;
  struct rbox_storage *rbox_ctx = (struct rbox_storage *)storage;

  rbox_dbg_print_mail_save_context(_ctx, "rbox_save_continue", NULL);
  rbox_dbg_print_mail_storage(storage, "rbox_save_continue", NULL);
  rbox_dbg_print_mail_user(storage->user, "rbox_save_continue", NULL);

  if (ctx->failed) {
    FUNC_END_RET("ret == -1; ctx failed");
    return -1;
  }

  if (_ctx->data.attach != NULL) {
    FUNC_END_RET("ret == index_attachment_save_continue");
    return index_attachment_save_continue(_ctx);
  }

  /* temporary guid generation see rbox-mail.c */
  char oid[GUID_128_SIZE];
  generate_oid(oid, _ctx->transaction->box->storage, ctx->seq);
  int bytes_written = 0;
  do {
    bytes_written = stream_mail_to_rados(rbox_ctx, oid, ctx->input);

    if (bytes_written < 0) {
      if (!mail_storage_set_error_from_errno(storage)) {
        mail_storage_set_critical(storage, "write(%s) failed: %m", o_stream_get_name(_ctx->data.output));
      }
      ctx->failed = TRUE;
      FUNC_END_RET("ret == -1; o_stream_send_istream failed");
      return -1;
    }
    index_mail_cache_parse_continue(_ctx->dest_mail);

    // both tee input readers may consume data from our primary
    // input stream. we'll have to make sure we don't return with
    //  one of the streams still having data in them.
  } while (i_stream_read(ctx->input) > 0);

  FUNC_END();
  return 0;
}

static int dbox_save_mail_write_metadata(struct dbox_save_context *ctx, struct dbox_file *file) {
  FUNC_START();
  struct rbox_file *sfile = (struct rbox_file *)file;
  const ARRAY_TYPE(mail_attachment_extref) * extrefs_arr;
  const struct mail_attachment_extref *extrefs;
  struct dbox_message_header dbox_msg_hdr;
  uoff_t message_size;
  guid_128_t guid_128;
  unsigned int i, count;

  rbox_dbg_print_dbox_file(file, "dbox_save_mail_write_metadata", NULL);
  rbox_dbg_print_mail_save_context(&ctx->ctx, "dbox_save_mail_write_metadata", NULL);

  i_assert(file->msg_header_size == sizeof(dbox_msg_hdr));

  message_size = ctx->dbox_output->offset - file->msg_header_size - file->file_header_size;

  dbox_save_write_metadata(&ctx->ctx, ctx->dbox_output, message_size, NULL, guid_128);
  dbox_msg_header_fill(&dbox_msg_hdr, message_size);
  if (o_stream_pwrite(ctx->dbox_output, &dbox_msg_hdr, sizeof(dbox_msg_hdr), file->file_header_size) < 0) {
    dbox_file_set_syscall_error(file, "pwrite()");
    FUNC_END_RET("ret == -1; o_stream_pwrite failed");
    return -1;
  }
  sfile->written_to_disk = TRUE;

  /* remember the attachment paths until commit time */
  extrefs_arr = index_attachment_save_get_extrefs(&ctx->ctx);
  if (extrefs_arr != NULL) {
    extrefs = array_get(extrefs_arr, &count);
  } else {
    extrefs = NULL;
    count = 0;
  }
  if (count > 0) {
    sfile->attachment_pool = pool_alloconly_create("rbox attachment paths", 512);
    p_array_init(&sfile->attachment_paths, sfile->attachment_pool, count);
    for (i = 0; i < count; i++) {
      const char *path = p_strdup(sfile->attachment_pool, extrefs[i].path);
      array_append(&sfile->attachment_paths, &path, 1);
    }
  }
  FUNC_END();
  return 0;
}

static int dbox_save_finish_write(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *ctx = (struct rbox_save_context *)_ctx;
  struct dbox_file **files;

  rbox_dbg_print_mail_save_context(_ctx, "dbox_save_finish_write", NULL);

  ctx->ctx.finished = TRUE;
  if (ctx->ctx.dbox_output == NULL) {
    FUNC_END_RET("ret == -1; ctx.dbox_output == NULL");
    return -1;
  }

  if (_ctx->data.save_date != (time_t)-1) {
    /* we can't change ctime, but we can add the date to cache */
    struct index_mail *mail = (struct index_mail *)_ctx->dest_mail;
    uint32_t t = _ctx->data.save_date;

    index_mail_cache_add(mail, MAIL_CACHE_SAVE_DATE, &t, sizeof(t));
  }
  dbox_save_end(&ctx->ctx);

  files = array_idx_modifiable(&ctx->files, array_count(&ctx->files) - 1);
  if (!ctx->ctx.failed) {
    T_BEGIN {
      if (dbox_save_mail_write_metadata(&ctx->ctx, *files) < 0)
        ctx->ctx.failed = TRUE;
    }
    T_END;
  }

  if (ctx->ctx.failed) {
    mail_index_expunge(ctx->ctx.trans, ctx->ctx.seq);
    mail_cache_transaction_reset(ctx->ctx.ctx.transaction->cache_trans);
    dbox_file_append_rollback(&ctx->append_ctx);
    dbox_file_unlink(*files);
    dbox_file_unref(files);
    array_delete(&ctx->files, array_count(&ctx->files) - 1, 1);
  } else {
    dbox_file_append_checkpoint(ctx->append_ctx);
    if (dbox_file_append_commit(&ctx->append_ctx) < 0)
      ctx->ctx.failed = TRUE;
    dbox_file_close(*files);
  }

  i_stream_unref(&ctx->ctx.input);
  ctx->ctx.dbox_output = NULL;

  FUNC_END();
  return ctx->ctx.failed ? -1 : 0;
}

int rbox_save_finish(struct mail_save_context *ctx) {
  FUNC_START();
  int ret;

  ret = dbox_save_finish_write(ctx);
  index_save_context_free(ctx);
  FUNC_END();
  return ret;
}

void rbox_save_cancel(struct mail_save_context *_ctx) {
  FUNC_START();
  struct dbox_save_context *ctx = (struct dbox_save_context *)_ctx;

  ctx->failed = TRUE;
  (void)rbox_save_finish(_ctx);
  FUNC_END();
}

static int dbox_save_assign_uids(struct rbox_save_context *ctx, const ARRAY_TYPE(seq_range) * uids) {
  FUNC_START();
  struct dbox_file *const *files;
  struct seq_range_iter iter;
  unsigned int i, count, n = 0;
  uint32_t uid;
  bool ret;

  rbox_dbg_print_mail_save_context(&ctx->ctx.ctx, "dbox_save_assign_uids", NULL);

  seq_range_array_iter_init(&iter, uids);
  files = array_get(&ctx->files, &count);
  for (i = 0; i < count; i++) {
    struct rbox_file *sfile = (struct rbox_file *)files[i];

    ret = seq_range_array_iter_nth(&iter, n++, &uid);
    i_assert(ret);
    if (rbox_file_assign_uid(sfile, uid, FALSE) < 0) {
      FUNC_END_RET("ret == -1; file_assign_uid failed");
      return -1;
    }
    if (ctx->ctx.highest_pop3_uidl_seq == i + 1) {
      index_pop3_uidl_set_max_uid(&ctx->mbox->box, ctx->ctx.trans, uid);
    }
  }
  i_assert(!seq_range_array_iter_nth(&iter, n, &uid));
  FUNC_END();
  return 0;
}

static int dbox_save_assign_stub_uids(struct rbox_save_context *ctx) {
  FUNC_START();
  struct dbox_file *const *files;
  unsigned int i, count;

  rbox_dbg_print_mail_save_context(&ctx->ctx.ctx, "dbox_save_assign_uids", NULL);

  files = array_get(&ctx->files, &count);
  for (i = 0; i < count; i++) {
    struct rbox_file *sfile = (struct rbox_file *)files[i];
    uint32_t uid;

    mail_index_lookup_uid(ctx->ctx.trans->view, ctx->first_saved_seq + i, &uid);
    i_assert(uid != 0);

    if (rbox_file_assign_uid(sfile, uid, TRUE) < 0) {
      FUNC_END_RET("ret == -1; file_assign_uid failed");
      return -1;
    }
  }

  FUNC_END();
  return 0;
}

static void dbox_save_unref_files(struct rbox_save_context *ctx) {
  FUNC_START();
  struct dbox_file **files;
  unsigned int i, count;

  rbox_dbg_print_mail_save_context(&ctx->ctx.ctx, "dbox_save_assign_uids", NULL);

  files = array_get_modifiable(&ctx->files, &count);
  for (i = 0; i < count; i++) {
    if (ctx->ctx.failed) {
      struct rbox_file *sfile = (struct rbox_file *)files[i];

      (void)rbox_file_unlink_aborted_save(sfile);
    }
    dbox_file_unref(&files[i]);
  }
  array_free(&ctx->files);
  FUNC_END();
}

int rbox_transaction_save_commit_pre(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *ctx = (struct rbox_save_context *)_ctx;
  struct mailbox_transaction_context *_t = _ctx->transaction;
  const struct mail_index_header *hdr;

  rbox_dbg_print_mail_save_context(_ctx, "rbox_transaction_save_commit_pre", NULL);

  i_assert(ctx->ctx.finished);

  if (array_count(&ctx->files) == 0) {
    /* the mail must be freed in the commit_pre() */
    FUNC_END_RET("ret == 0; array_count == 0");
    return 0;
  }

  if (rbox_sync_begin(ctx->mbox, RBOX_SYNC_FLAG_FORCE | RBOX_SYNC_FLAG_FSYNC, &ctx->sync_ctx) < 0) {
    rbox_transaction_save_rollback(_ctx);
    FUNC_END_RET("ret == -1; sync_begin failed");
    return -1;
  }

  /* update dbox header flags */
  dbox_save_update_header_flags(&ctx->ctx, ctx->sync_ctx->sync_view, ctx->mbox->hdr_ext_id,
                                offsetof(struct rbox_index_header, flags));

  hdr = mail_index_get_header(ctx->sync_ctx->sync_view);

  if ((_ctx->transaction->flags & MAILBOX_TRANSACTION_FLAG_FILL_IN_STUB) == 0) {
    /* assign UIDs for new messages */
    mail_index_append_finish_uids(ctx->ctx.trans, hdr->next_uid, &_t->changes->saved_uids);
    if (dbox_save_assign_uids(ctx, &_t->changes->saved_uids) < 0) {
      rbox_transaction_save_rollback(_ctx);
      FUNC_END_RET("ret == -1; assign_uids failed");
      return -1;
    }
  } else {
    /* assign UIDs that we stashed away */
    if (dbox_save_assign_stub_uids(ctx) < 0) {
      rbox_transaction_save_rollback(_ctx);
      FUNC_END_RET("ret == -1; assign_stub_uids failed");
      return -1;
    }
  }

  _t->changes->uid_validity = hdr->uid_validity;
  FUNC_END();
  return 0;
}

void rbox_transaction_save_commit_post(struct mail_save_context *_ctx,
                                       struct mail_index_transaction_commit_result *result) {
  FUNC_START();
  struct rbox_save_context *ctx = (struct rbox_save_context *)_ctx;
  struct mail_storage *storage = _ctx->transaction->box->storage;

  _ctx->transaction = NULL; /* transaction is already freed */

  rbox_dbg_print_mail_save_context(_ctx, "rbox_transaction_save_commit_post", NULL);

  if (array_count(&ctx->files) == 0) {
    rbox_transaction_save_rollback(_ctx);
    FUNC_END_RET("array_count == 0");
    return;
  }

  mail_index_sync_set_commit_result(ctx->sync_ctx->index_sync_ctx, result);

  if (rbox_sync_finish(&ctx->sync_ctx, TRUE) < 0)
    ctx->ctx.failed = TRUE;

  if (storage->set->parsed_fsync_mode != FSYNC_MODE_NEVER) {
    const char *box_path = mailbox_get_path(&ctx->mbox->box);

    if (fdatasync_path(box_path) < 0) {
      mail_storage_set_critical(storage, "fdatasync_path(%s) failed: %m", box_path);
    }
  }
  rbox_transaction_save_rollback(_ctx);
  FUNC_END();
}

void rbox_transaction_save_rollback(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *ctx = (struct rbox_save_context *)_ctx;

  if (!ctx->ctx.finished)
    rbox_save_cancel(_ctx);
  dbox_save_unref_files(ctx);

  if (ctx->sync_ctx != NULL)
    (void)rbox_sync_finish(&ctx->sync_ctx, FALSE);
  i_free(ctx);
  FUNC_END();
}
