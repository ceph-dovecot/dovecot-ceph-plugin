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

#include <stdio.h>
#include <utime.h>
#include <time.h>

#include <string>
#include <map>
#include <vector>
#include <rados/librados.hpp>

extern "C" {
#include "dovecot-all.h"

#include "istream.h"
#include "istream-crlf.h"
#include "ostream.h"
#include "str.h"

#include "rbox-sync.h"

#include "debug-helper.h"
}

#include "rados-mail-object.h"
#include "rbox-storage.hpp"
#include "rbox-save.h"
#include "rados-util.h"
#include "rbox-mail.h"

using ceph::bufferlist;

using librmb::RadosStorage;
using librmb::RadosMailObject;
using librmb::RadosMetadata;
using librmb::rbox_metadata_key;

using std::string;
using std::vector;

struct mail_save_context *rbox_save_alloc(struct mailbox_transaction_context *t) {
  FUNC_START();
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)t->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)rbox->storage;
  rbox_save_context *r_ctx;

  i_assert((t->flags & MAILBOX_TRANSACTION_FLAG_EXTERNAL) != 0);

  if (t->save_ctx == NULL) {
    i_debug("rbox_save_alloc: t->save_ctx == NULL, mailbox name = %s", t->box->name);
    r_ctx = new rbox_save_context(*(r_storage->s));
    r_ctx->ctx.transaction = t;
    r_ctx->mbox = rbox;
    r_ctx->trans = t->itrans;
    r_ctx->current_object = nullptr;
    t->save_ctx = &r_ctx->ctx;
  }

  FUNC_END();
  return t->save_ctx;
}

void rbox_add_to_index(struct mail_save_context *_ctx) {
  FUNC_START();
  struct mail_save_data *mdata = &_ctx->data;
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  int save_flags;

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
  guid_128_generate(r_ctx->mail_oid);

  r_ctx->current_object = r_storage->s->alloc_mail_object();
  r_ctx->current_object->set_oid(guid_128_to_string(r_ctx->mail_oid));
  r_ctx->objects.push_back(r_ctx->current_object);

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
  FUNC_END();
}

void rbox_move_index(struct mail_save_context *_ctx, struct mail *src_mail) {
  struct mail_save_data *mdata = &_ctx->data;
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;
  int save_flags;

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

#ifdef DOVECOT_CEPH_PLUGINS_HAVE_MAIL_SAVE_CONTEXT_COPY_SRC_MAIL
  struct rbox_mail *r_src_mail = (struct rbox_mail *)ctx->copy_src_mail;
#else
  struct rbox_mail *r_src_mail = (struct rbox_mail *)src_mail;
#endif
  guid_128_from_string(r_src_mail->mail_object->get_oid().c_str(), r_ctx->mail_oid);

  r_ctx->current_object = r_storage->s->alloc_mail_object();
  r_ctx->current_object->set_oid(guid_128_to_string(r_ctx->mail_oid));
  r_ctx->objects.push_back(r_ctx->current_object);

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

  if (_ctx->dest_mail != NULL) {
    i_debug("SAVE OID: %s, %d uid, seq=%d", guid_128_to_string(rec.oid), _ctx->dest_mail->uid, r_ctx->seq);
    mail_set_seq_saving(_ctx->dest_mail, r_ctx->seq);
  }
}
void init_output_stream(mail_save_context *_ctx) {
  FUNC_START();
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  if (_ctx->data.output != NULL) {
    o_stream_unref(&_ctx->data.output);
  }

  r_ctx->output_stream = o_stream_create_buffer(reinterpret_cast<buffer_t *>(r_ctx->current_object->get_mail_buffer()));
  o_stream_cork(r_ctx->output_stream);
  _ctx->data.output = r_ctx->output_stream;
  FUNC_END();
}

int allocate_mail_buffer(mail_save_context *_ctx, int &initial_mail_buffer_size) {
  FUNC_START();
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  librmb::RadosMailObject *mail = r_ctx->current_object;
  char *current_mail_buffer = mail->get_mail_buffer();

  if (current_mail_buffer != NULL) {
    buffer_t *buffer = reinterpret_cast<buffer_t *>(current_mail_buffer);
    // make 100% sure, buffer is empty!
    buffer_free(&buffer);
  }
  mail->set_mail_buffer(reinterpret_cast<char *>(buffer_create_dynamic(default_pool, initial_mail_buffer_size)));

  if (mail->get_mail_buffer() == NULL) {
    FUNC_END_RET("ret == -1");
    return -1;
  }
  FUNC_END();
  return 0;
}

int rbox_save_begin(struct mail_save_context *_ctx, struct istream *input) {
  FUNC_START();
  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct istream *crlf_input;
  int initial_mail_buffer_size = 512;

  r_ctx->failed = FALSE;
  if (_ctx->dest_mail == NULL) {
    _ctx->dest_mail = mail_alloc(_ctx->transaction, static_cast<mail_fetch_field>(0), NULL);
    r_ctx->dest_mail_allocated = TRUE;
  }

  rbox_add_to_index(_ctx);

  mail_set_seq_saving(_ctx->dest_mail, r_ctx->seq);
  crlf_input = i_stream_create_crlf(input);
  r_ctx->input = index_mail_cache_parse_init(_ctx->dest_mail, crlf_input);
  i_stream_unref(&crlf_input);

  int ret = allocate_mail_buffer(_ctx, initial_mail_buffer_size);
  if (ret < 0) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  init_output_stream(_ctx);

  if (_ctx->data.received_date == (time_t)-1)
    _ctx->data.received_date = ioloop_time;

  i_debug("SAVE OID: %s, %d uid, seq=%d", guid_128_to_string(r_ctx->mail_oid), _ctx->dest_mail->uid, r_ctx->seq);

  FUNC_END();
  return 0;
}

int rbox_save_continue(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;

  if (r_ctx->failed) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  do {
    if (o_stream_send_istream(_ctx->data.output, r_ctx->input) < 0) {
      if (!mail_storage_set_error_from_errno(storage)) {
        mail_storage_set_critical(storage, "write(%s) failed: %m", o_stream_get_name(_ctx->data.output));
      }
      r_ctx->failed = TRUE;
      FUNC_END_RET("ret == -1");
      return -1;
    }

    index_mail_cache_parse_continue(_ctx->dest_mail);
    /* both tee input readers may consume data from our primary
     input stream. we'll have to make sure we don't return with
     one of the streams still having data in them. */
  } while (i_stream_read(r_ctx->input) > 0);

  FUNC_END();
  return 0;
}

static int rbox_save_mail_set_metadata(struct rbox_save_context *ctx, librmb::RadosMailObject *mail_object) {
  FUNC_START();
  struct mail_save_data *mdata = &ctx->ctx.data;

  struct rbox_storage *r_storage = (struct rbox_storage *)&ctx->mbox->storage->storage;

  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_VERSION)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_VERSION, RadosMailObject::X_ATTR_VERSION_VALUE);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID, guid_128_to_string(ctx->mbox->mailbox_guid));
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_GUID)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_GUID, guid_128_to_string(ctx->mail_guid));
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_RECEIVED_TIME)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_RECEIVED_TIME, mdata->received_date);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_POP3_UIDL)) {
    if (mdata->pop3_uidl != NULL) {
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_POP3_UIDL, mdata->pop3_uidl);
      mail_object->add_metadata(xattr);
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_POP3_ORDER)) {
    if (mdata->pop3_order != 0) {
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_POP3_ORDER, mdata->pop3_order);
      mail_object->add_metadata(xattr);
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_FROM_ENVELOPE)) {
    if (mdata->from_envelope != NULL) {
      i_debug("from envelope %s", mdata->from_envelope);
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_FROM_ENVELOPE, mdata->from_envelope);
      mail_object->add_metadata(xattr);
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE)) {
    uoff_t vsize = -1;
    if (mail_get_virtual_size(ctx->ctx.dest_mail, &vsize) < 0) {
      i_debug("warning, unable to determine virtual size, using physical size instead.");
      vsize = ctx->input->v_offset;
    }
    librmb::RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE, vsize);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_PHYSICAL_SIZE)) {
    librmb::RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_PHYSICAL_SIZE, ctx->input->v_offset);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_OLDV1_FLAGS)) {
    if (mdata->flags != 0) {
      std::string flags = librmb::RadosUtils::flags_to_string(mdata->flags);
      // i_debug("%s :flags : value=%s", mail_object->get_oid().c_str(), flags.c_str());
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_OLDV1_FLAGS, flags);
      mail_object->add_metadata(xattr);
      // i_debug("save_flags : %s,  %s", mail_object->get_oid().c_str(), flags.c_str());
    }
  }

  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_PVT_FLAGS)) {
    if (mdata->pvt_flags != 0) {
      std::string pvt_flags = librmb::RadosUtils::flags_to_string(mdata->pvt_flags);
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_PVT_FLAGS, pvt_flags);
      mail_object->add_metadata(xattr);
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, ctx->mbox->box.name);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_OLDV1_KEYWORDS)) {
    const char *const *keywords_list = mail_get_keywords(ctx->ctx.dest_mail);
    struct mail_keywords *keywords;
    keywords = str_array_length(keywords_list) == 0 ? NULL : mailbox_keywords_create_valid(ctx->ctx.transaction->box,
                                                                                           keywords_list);
    unsigned int keyword_count = keywords == NULL ? 0 : keywords->count;
    if (keyword_count > 0) {
      for (unsigned int i = 0; i < keyword_count; i++) {
        std::string keyword = std::to_string(keywords->idx[i]);
        std::string ext_key = "k_" + keyword;
        RadosMetadata ext_metadata(ext_key, keyword);
        mail_object->add_extended_metadata(ext_metadata);
        //  i_debug("keyword_added %s, %d:  '%s' ", mail_object->get_oid().c_str(), i, keyword.c_str());
      }
    }

    if (keywords != NULL)
      mailbox_keywords_unref(&keywords);
  }

  mail_object->set_rados_save_date(mdata->save_date);

  FUNC_END();
  return 0;
}

static void clean_up_failed(struct rbox_save_context *r_ctx) {
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  r_storage->s->wait_for_rados_operations(r_ctx->objects);

  for (std::vector<RadosMailObject *>::iterator it_cur_obj = r_ctx->objects.begin(); it_cur_obj != r_ctx->objects.end();
       ++it_cur_obj) {
    if (r_storage->s->delete_mail(*it_cur_obj) < 0) {
      i_debug("Librados obj: %s, could not be removed", (*it_cur_obj)->get_oid().c_str());
    }
  }
  // clean up index
  if (r_ctx->seq > 0) {
    mail_index_expunge(r_ctx->trans, r_ctx->seq);
  }
  mail_cache_transaction_reset(r_ctx->ctx.transaction->cache_trans);

  clean_up_mail_object_list(r_ctx, r_storage);
  r_ctx->mail_count--;
}

static void clean_up_write_finish(struct mail_save_context *_ctx) {
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  if (r_ctx->input != NULL) {
    i_stream_unref(&r_ctx->input);
  }
  if (_ctx->data.output != NULL) {
    o_stream_unref(&_ctx->data.output);
  }

  index_save_context_free(_ctx);
}

int rbox_save_finish(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  r_ctx->finished = TRUE;
  if (!r_ctx->failed) {
    if (_ctx->data.save_date != (time_t)-1) {
      uint32_t save_date = _ctx->data.save_date;
      index_mail_cache_add((struct index_mail *)_ctx->dest_mail, MAIL_CACHE_SAVE_DATE, &save_date, sizeof(save_date));
    }

    struct mail_save_data *mdata = &r_ctx->ctx.data;
    if (mdata->output != r_ctx->output_stream) {
      /* e.g. zlib plugin had changed this */
      o_stream_ref(r_ctx->output_stream);
      o_stream_destroy(&mdata->output);
      mdata->output = r_ctx->output_stream;
    }
    // reset virtual size
    index_mail_cache_parse_deinit(_ctx->dest_mail, r_ctx->ctx.data.received_date, !r_ctx->failed);

    if (rbox_open_rados_connection(_ctx->transaction->box) < 0) {
      i_error("ERROR, cannot open rados connection (rbox_save_finish)");
      r_ctx->failed = true;
    } else {
      bool async_write = true;
      // build rbox_mail_object
      buffer_t *mail_buffer = reinterpret_cast<buffer_t *>(r_ctx->current_object->get_mail_buffer());
      r_ctx->current_object->set_mail_buffer_content_ptr(mail_buffer->data);
      r_ctx->current_object->set_mail_size(buffer_get_used_size(mail_buffer));

      rbox_save_mail_set_metadata(r_ctx, r_ctx->current_object);
      r_ctx->failed = !r_storage->s->save_mail(r_ctx->current_object, async_write);
      if (r_ctx->failed) {
        i_error("saved mail: %s failed metadata_count %lu", r_ctx->current_object->get_oid().c_str(),
                r_ctx->current_object->get_metadata()->size());
      }
    }
  }

  clean_up_write_finish(_ctx);
  FUNC_END();
  return r_ctx->failed ? -1 : 0;
}

void rbox_save_cancel(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  r_ctx->failed = TRUE;
  (void)rbox_save_finish(_ctx);
  clean_up_failed(r_ctx);
  FUNC_END();
}

static int rbox_save_assign_uids(struct rbox_save_context *r_ctx, const ARRAY_TYPE(seq_range) * uids) {
  struct seq_range_iter iter;
  unsigned int n = 0;
  uint32_t uid = -1;
  bool ret = false;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  seq_range_array_iter_init(&iter, uids);

  for (std::vector<RadosMailObject *>::iterator it = r_ctx->objects.begin(); it != r_ctx->objects.end(); ++it) {
    r_ctx->current_object = *it;
    ret = seq_range_array_iter_nth(&iter, n++, &uid);
    i_assert(ret);
    {
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_MAIL_UID, uid);
      int ret_val = r_storage->s->set_metadata(r_ctx->current_object->get_oid(), xattr);
      if (ret_val < 0) {
        return -1;
      }
    }
  }
  i_assert(!seq_range_array_iter_nth(&iter, n, &uid));
  return 0;
}

int rbox_transaction_save_commit_pre(struct mail_save_context *_ctx) {
  FUNC_START();
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct mailbox_transaction_context *_t = _ctx->transaction;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  const struct mail_index_header *hdr;
  struct seq_range_iter iter;

  i_assert(r_ctx->finished);

  r_ctx->failed = r_storage->s->wait_for_rados_operations(r_ctx->objects);

  // if one write fails! all writes will be reverted and r_ctx->failed is true!
  if (r_ctx->failed) {
    // delete index entry and delete object if it exist
    // remove entry from index is not successful in rbox_transaction_commit_post
    // clean up will wait for object operation to complete
    i_error("rados wait_for_rados_operation failed: ");
    rbox_transaction_save_rollback(_ctx);
    return -1;
  }
  int sync_flags = RBOX_SYNC_FLAG_FORCE | RBOX_SYNC_FLAG_FSYNC;
  if (rbox_sync_begin(r_ctx->mbox, &r_ctx->sync_ctx, static_cast<rbox_sync_flags>(sync_flags)) < 0) {
    r_ctx->failed = TRUE;
    rbox_transaction_save_rollback(_ctx);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  hdr = mail_index_get_header(r_ctx->sync_ctx->sync_view);
  mail_index_append_finish_uids(r_ctx->trans, hdr->next_uid, &_t->changes->saved_uids);
  _t->changes->uid_validity = r_ctx->sync_ctx->uid_validity;

  seq_range_array_iter_init(&iter, &_t->changes->saved_uids);
  if (rbox_save_assign_uids(r_ctx, &_t->changes->saved_uids) < 0) {
    rbox_transaction_save_rollback(_ctx);
    return -1;
  }

  if (_ctx->dest_mail != NULL) {
    if (r_ctx->dest_mail_allocated == TRUE) {
      mail_free(&_ctx->dest_mail);
      r_ctx->dest_mail_allocated = FALSE;
    } else {
      _ctx->dest_mail = NULL;
    }
  }
  _t->changes->uid_validity = hdr->uid_validity;

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
  rbox_transaction_save_rollback(_ctx);

  FUNC_END();
}

void clean_up_mail_object_list(struct rbox_save_context *r_ctx, struct rbox_storage *r_storage) {
  for (std::vector<RadosMailObject *>::iterator it = r_ctx->objects.begin(); it != r_ctx->objects.end(); ++it) {
    buffer_t *mail_buffer = reinterpret_cast<buffer_t *>((*it)->get_mail_buffer());
    if (mail_buffer != NULL) {
      buffer_free(&mail_buffer);
      mail_buffer = NULL;
      (*it)->set_mail_buffer(NULL);
    }
    r_storage->s->free_mail_object(*it);
    *it = nullptr;
  }
  r_ctx->objects.clear();
}

void rbox_transaction_save_rollback(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  if (!r_ctx->finished) {
    rbox_save_cancel(&r_ctx->ctx);
  }

  if (r_ctx->sync_ctx != NULL)
    (void)rbox_sync_finish(&r_ctx->sync_ctx, FALSE);

  if (r_ctx->failed) {
    // delete index entry and delete object if it exist
    clean_up_failed(r_ctx);
  }

  clean_up_mail_object_list(r_ctx, r_storage);

  guid_128_empty(r_ctx->mail_guid);
  guid_128_empty(r_ctx->mail_oid);
  if (_ctx->dest_mail != NULL && r_ctx->dest_mail_allocated == TRUE) {
    mail_free(&_ctx->dest_mail);
  }
  r_ctx->current_object = nullptr;
  delete r_ctx;
  FUNC_END();
}
