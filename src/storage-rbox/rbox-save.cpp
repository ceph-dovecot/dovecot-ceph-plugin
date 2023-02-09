// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 * Copyright (c) 2007-2017 Dovecot authors
 *
 * This is free software; you can redistribute it and/or
 *
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include <stdio.h>
#include <utime.h>
#include <time.h>

#include <string>
#include <map>
#include <list>
#include <rados/librados.hpp>

extern "C" {
#include "dovecot-all.h"
#include "macros.h"
#include "istream.h"
#include "istream-crlf.h"
#include "ostream.h"
#include "str.h"

#include "rbox-sync.h"
#include "rados-types.h"
#include "debug-helper.h"
#if DOVECOT_PREREQ(2, 3)
#include "index-pop3-uidl.h"
#endif
}

#include "../librmb/rados-mail.h"
#include "rbox-storage.hpp"
#include "rbox-save.h"
#include "rados-util.h"
#include "rbox-mail.h"
#include "ostream-bufferlist.h"

using ceph::bufferlist;

using librmb::RadosMail;
using librmb::RadosMetadata;
using librmb::RadosStorage;
using librmb::rbox_metadata_key;

using std::string;

static const char X_ATTR_VERSION_VALUE[] = "0.1";

struct mail_save_context *rbox_save_alloc(struct mailbox_transaction_context *t) {
  FUNC_START();

  struct rbox_mailbox *rbox = (struct rbox_mailbox *)t->box;
  struct rbox_storage *r_storage = (struct rbox_storage *)rbox->storage;
  struct rbox_save_context *r_ctx = (struct rbox_save_context *)t->save_ctx;

  i_assert((t->flags & MAILBOX_TRANSACTION_FLAG_EXTERNAL) != 0);

  if (r_ctx == 0 || r_ctx == NULL) {
    r_ctx = new rbox_save_context(*(r_storage->s));
    r_ctx->ctx.transaction = t;
    r_ctx->mbox = rbox;
    r_ctx->trans = t->itrans;
    t->save_ctx = &r_ctx->ctx;
  } else {
    r_ctx->failed = FALSE;
    r_ctx->finished = FALSE;
    r_ctx->output_stream = NULL;
  }
  r_ctx->rados_mail = nullptr;

  FUNC_END();
  return t->save_ctx;
}

void setup_mail_object(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  guid_128_generate(r_ctx->mail_oid);
  r_ctx->rados_mail = ((struct rbox_storage *)&r_ctx->mbox->storage->storage)->s->alloc_rados_mail();
  r_ctx->rados_mail->set_oid(guid_128_to_string(r_ctx->mail_oid));

  if (_ctx->data.guid != NULL) {
    string str(_ctx->data.guid);
    //#286: use deprecated_flag to save original uuid format to G xattr
    if (str.find("-")<str.length()) {
      r_ctx->rados_mail->set_deprecated_uid(true);
    }
    librmb::RadosUtils::find_and_replace(&str, "-", "");  // remove hyphens if they exist
    mail_generate_guid_128_hash(str.c_str(), r_ctx->mail_guid);
  } else {
    guid_128_generate(r_ctx->mail_guid);
  }

  FUNC_END();
}

void rbox_index_append(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  mail_index_append(r_ctx->trans, _ctx->data.uid, &r_ctx->seq);

  i_debug("added index for seq: %d",r_ctx->seq);

  mail_index_update_flags(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE,
                          static_cast<enum mail_flags>(_ctx->data.flags & ~MAIL_RECENT));

  if (_ctx->data.keywords != NULL) {
    mail_index_update_keywords(r_ctx->trans, r_ctx->seq, MODIFY_REPLACE, _ctx->data.keywords);
  }

  if (_ctx->data.min_modseq != 0) {
    mail_index_update_modseq(r_ctx->trans, r_ctx->seq, _ctx->data.min_modseq);
  }

  FUNC_END();
}

void rbox_add_to_index(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  /* add to index */
  rbox_index_append(_ctx);

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  memcpy(rec.guid, r_ctx->mail_guid, sizeof(r_ctx->mail_guid));
  memcpy(rec.oid, r_ctx->mail_oid, sizeof(r_ctx->mail_oid));

  mail_index_update_ext(r_ctx->trans, r_ctx->seq, r_ctx->mbox->ext_id, &rec, NULL);
  r_ctx->rados_mails.push_back(r_ctx->rados_mail);

  FUNC_END();
}

void rbox_move_index(struct mail_save_context *_ctx, struct mail *src_mail) {
  FUNC_START();

  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  /* add to index */
  rbox_index_append(_ctx);

#ifdef DOVECOT_CEPH_PLUGINS_HAVE_MAIL_SAVE_CONTEXT_COPY_SRC_MAIL
  struct rbox_mail *r_src_mail = (struct rbox_mail *)ctx->copy_src_mail;
#else
  struct rbox_mail *r_src_mail = (struct rbox_mail *)src_mail;
#endif

  guid_128_from_string(r_src_mail->rados_mail->get_oid()->c_str(), r_ctx->mail_oid);

  r_ctx->rados_mail = r_storage->s->alloc_rados_mail();
  r_ctx->rados_mail->set_oid(guid_128_to_string(r_ctx->mail_oid));
  r_ctx->rados_mails.push_back(r_ctx->rados_mail);

  if (_ctx->data.guid != NULL) {
    string str(_ctx->data.guid);
    //#286: use deprecated_flag to save original uuid format to G xattr
    if (str.find("-")<str.length()) {
      r_ctx->rados_mail->set_deprecated_uid(true);
    }
    librmb::RadosUtils::find_and_replace(&str, "-", "");  // remove hyphens if they exist
    mail_generate_guid_128_hash(str.c_str(), r_ctx->mail_guid);
  } else {
    guid_128_generate(r_ctx->mail_guid);
  }

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  memcpy(rec.guid, r_ctx->mail_guid, sizeof(r_ctx->mail_guid));
  memcpy(rec.oid, r_ctx->mail_oid, sizeof(r_ctx->mail_oid));
  mail_index_update_ext(r_ctx->trans, r_ctx->seq, r_ctx->mbox->ext_id, &rec, NULL);

  if (_ctx->dest_mail != NULL) {
    mail_set_seq_saving(_ctx->dest_mail, r_ctx->seq);
  }

  FUNC_END();
}
void init_output_stream(mail_save_context *_ctx) {
  FUNC_START();

  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)_ctx->transaction->box;
  if (_ctx->data.output != NULL) {
    i_debug("freing data output stream!");
    o_stream_unref(&_ctx->data.output);
  }

  // create buffer ( delete is in wait_for_write_operations)
  r_ctx->rados_mail->set_mail_buffer(new librados::bufferlist());
  r_ctx->output_stream =
      o_stream_create_bufferlist(r_ctx->rados_mail, &r_ctx->rados_storage, rbox->storage->config->is_write_chunks());
  o_stream_cork(r_ctx->output_stream);
  _ctx->data.output = r_ctx->output_stream;

  FUNC_END();
}

int rbox_save_begin(struct mail_save_context *_ctx, struct istream *input) {
  FUNC_START();

  rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  r_ctx->failed = FALSE;

  if (_ctx->dest_mail == NULL) {
    _ctx->dest_mail = mail_alloc(_ctx->transaction, static_cast<mail_fetch_field>(0), NULL);
    r_ctx->dest_mail_allocated = TRUE;
  }
  setup_mail_object(_ctx);

  // init stream in any case.
  init_output_stream(_ctx);

  // always save to primary storage
  if (rbox_open_rados_connection(_ctx->transaction->box, false) < 0) {
    i_error("ERROR, cannot open rados connection (rbox_save_finish)");
    r_ctx->failed = true;
  } else {
    // we cannot check for mailsize > ceph object size here,
    // as we do not know the real mailsize
    rbox_add_to_index(_ctx);
    mail_set_seq_saving(_ctx->dest_mail, r_ctx->seq);

    struct istream *crlf_input;

#ifdef HAVE_SAVE_FROM_LF_SRC
    if (_ctx->save_from_lf_src) {
      crlf_input = i_stream_create_crlf(input);
    }
    else {
      crlf_input = i_stream_create_lf(input);
    }
#else
    crlf_input = i_stream_create_lf(input);
#endif

    r_ctx->input = index_mail_cache_parse_init(_ctx->dest_mail, crlf_input);
    i_stream_unref(&crlf_input);

    if (_ctx->data.received_date == (time_t)-1) {
      _ctx->data.received_date = ioloop_time;
    }
  }

  FUNC_END();
  return 0;
}

int rbox_save_continue(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  if (r_ctx->failed) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

#if DOVECOT_PREREQ(2, 3)
  if (index_storage_save_continue(_ctx, r_ctx->input, _ctx->dest_mail) < 0) {
    r_ctx->failed = TRUE;
    FUNC_END();
    return -1;
  }
#else
  struct mail_storage *storage = &r_ctx->mbox->storage->storage;
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
     one of the streams still having data in them.*/
  } while (i_stream_read(r_ctx->input) > 0);

#endif

  FUNC_END();
  return 0;
}

static int rbox_save_mail_set_metadata(struct rbox_save_context *r_ctx, librmb::RadosMail *mail_object) {
  FUNC_START();

  struct mail_save_data *mdata = &r_ctx->ctx.data;
  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_VERSION)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_VERSION, X_ATTR_VERSION_VALUE);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_MAILBOX_GUID, guid_128_to_string(r_ctx->mbox->mailbox_guid));
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_GUID)) {
    //#286: use deprecated_flag to save original uuid format to G xattr
    if(!mail_object->is_deprecated_uid()){
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_GUID, guid_128_to_string(r_ctx->mail_guid));
      mail_object->add_metadata(xattr);
    }
    else{
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_GUID, guid_128_to_uuid_string(r_ctx->mail_guid,FORMAT_RECORD));
      mail_object->add_metadata(xattr);
    }    
  }
  
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_RECEIVED_TIME)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_RECEIVED_TIME, mdata->received_date);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_POP3_UIDL)) {
    if (mdata->pop3_uidl != NULL) {
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_POP3_UIDL, mdata->pop3_uidl);
      mail_object->add_metadata(xattr);
      r_ctx->have_pop3_uidls = TRUE;
#if DOVECOT_PREREQ(2, 3)
      r_ctx->highest_pop3_uidl_seq = I_MAX(r_ctx->highest_pop3_uidl_seq, r_ctx->seq);
#endif
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_POP3_ORDER)) {
    if (mdata->pop3_order != 0) {
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_POP3_ORDER, mdata->pop3_order);
      mail_object->add_metadata(xattr);
      r_ctx->have_pop3_orders = TRUE;
#if DOVECOT_PREREQ(2, 3)
      r_ctx->highest_pop3_uidl_seq = I_MAX(r_ctx->highest_pop3_uidl_seq, r_ctx->seq);
#endif
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_FROM_ENVELOPE)) {
    if (mdata->from_envelope != NULL) {
      RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_FROM_ENVELOPE, mdata->from_envelope);
      mail_object->add_metadata(xattr);
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE)) {
    uoff_t vsize = -1;
    if (mail_get_virtual_size(r_ctx->ctx.dest_mail, &vsize) < 0) {
      i_warning("unable to determine virtual size, using physical size instead.");
      vsize = r_ctx->input->v_offset;
    }
    librmb::RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE, vsize);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_PHYSICAL_SIZE)) {
    librmb::RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_PHYSICAL_SIZE, r_ctx->input->v_offset);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_OLDV1_FLAGS)) {
    if (mdata->flags != 0) {
      std::string flags;
      if (librmb::RadosUtils::flags_to_string(mdata->flags, &flags)) {
        RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_OLDV1_FLAGS, flags);
        mail_object->add_metadata(xattr);
      }
    }
  }

  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_PVT_FLAGS)) {
    if (mdata->pvt_flags != 0) {
      std::string pvt_flags;
      if (librmb::RadosUtils::flags_to_string(mdata->pvt_flags, &pvt_flags)) {
        RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_PVT_FLAGS, pvt_flags);
        mail_object->add_metadata(xattr);
      }
    }
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX)) {
    RadosMetadata xattr(rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, r_ctx->mbox->box.name);
    mail_object->add_metadata(xattr);
  }
  if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_OLDV1_KEYWORDS)) {
    struct rbox_mail *rmail = (struct rbox_mail *)r_ctx->ctx.dest_mail;
    // load keyword indexes to rmail->imal.data.keyword_indexes
    index_mail_get_keyword_indexes(r_ctx->ctx.dest_mail);
    unsigned int count_keyword_indexes;
    const unsigned int *const keyword_indexes = array_get(&rmail->imail.data.keyword_indexes, &count_keyword_indexes);

    // load keywords to rmail->imal.data.keywords
    // TODO(jrse): is mail_get_keywords really necessary?
    mail_get_keywords(r_ctx->ctx.dest_mail);
    unsigned int count_keywords;
    const char *const *keywords = array_get(&rmail->imail.data.keywords, &count_keywords);

    for (unsigned int i = 0; i < count_keyword_indexes; ++i) {
      // set keyword_idx : keyword_value
      std::string key_idx = std::to_string(keyword_indexes[i]);
      std::string keyword_value = keywords[i];
      RadosMetadata ext_metadata(key_idx, keyword_value);
      mail_object->add_extended_metadata(ext_metadata);
    }
  }

  mail_object->set_rados_save_date(mdata->save_date);

  FUNC_END();
  return 0;
}

static void clean_up_failed(struct rbox_save_context *r_ctx, bool wait_for_operations) {
  FUNC_START();

  struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

  // try to clean up!
  for (std::list<RadosMail *>::iterator it_cur_obj = r_ctx->rados_mails.begin(); it_cur_obj != r_ctx->rados_mails.end();
       ++it_cur_obj) {
    int delete_ret = r_storage->s->delete_mail(*it_cur_obj);
    if (delete_ret < 0 && delete_ret != -ENOENT) {
      i_error("Librados obj: %s, could not be removed", (*it_cur_obj)->get_oid()->c_str());
    }
  }
  
  // clean up index only if index entry was added.

  if (r_ctx->seq > 0) {
    i_debug("expunge index for seq: %d",r_ctx->seq);
    mail_index_expunge(r_ctx->trans, r_ctx->seq);
  }
  
  if (r_ctx->ctx.transaction != NULL) {
    mail_cache_transaction_reset(r_ctx->ctx.transaction->cache_trans);
  }
  
  clean_up_mail_object_list(r_ctx, r_storage);
  r_ctx->mail_count--;
  FUNC_END();
}

static void clean_up_write_finish(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  r_ctx->finished = TRUE;

  if (r_ctx->input != NULL) {
    i_stream_unref(&r_ctx->input);
  }
  if (_ctx->data.output != NULL) {
    o_stream_unref(&_ctx->data.output);
    _ctx->data.output = NULL;
  }

  index_save_context_free(_ctx);

  FUNC_END();
}


int save_mail_write_append(RadosStorage *rados_storage,
                             RadosMail *current_object,
                             librados::ObjectWriteOperation *write_op_xattr,
                             const uint64_t &max_write) {

  int ret_val = 0;
  uint64_t write_buffer_size = current_object->get_mail_size();

  assert(max_write > 0);

  if (write_buffer_size == 0 || max_write <= 0) {
    ret_val = -1;
    i_debug("write_buffer_size == 0 or max_write <=0 < -1" );
    return ret_val;
  }

  ret_val = rados_storage->execute_operation(*current_object->get_oid(), write_op_xattr);

  if(ret_val< 0){
    i_debug("write metadata did not work: %d",ret_val);
    ret_val = -1;
    return ret_val;
  }

  uint64_t rest = write_buffer_size % max_write;
  int div = write_buffer_size / max_write + (rest > 0 ? 1 : 0);
  for (int i = 0; i < div; ++i) {

    // split the buffer.
    librados::bufferlist tmp_buffer;

    librados::ObjectWriteOperation write_op;

    int offset = i * max_write;

    uint64_t length = max_write;
    if (write_buffer_size < ((i + 1) * length)) {
      length = rest;
    }

    if (div == 1) {
      write_op.write(0, *current_object->get_mail_buffer());
      ret_val = rados_storage->execute_operation(*current_object->get_oid(), &write_op) ? 0 : -1;
    } else {
      i_debug("write chunk size %d, offset=%d,lenght=%d",write_buffer_size,offset,length);      
      if(offset + length > write_buffer_size){
        i_error("offset and length (%d) is bigger then write_buffer size (%d)", (offset+length), write_buffer_size);
        return -1;
      }else{
        i_debug("trying to get substring of : mailsize %d, offset: %d, length %d",
            current_object->get_mail_buffer()->length(), offset, length );        
        if(offset + length > current_object->get_mail_buffer()->length() ){
           i_debug("new offset : %d",current_object->get_mail_buffer()->length()-offset);
           tmp_buffer.substr_of(*current_object->get_mail_buffer(), offset,current_object->get_mail_buffer()->length() - offset );
        }else{
          tmp_buffer.substr_of(*current_object->get_mail_buffer(), offset, length);
        }
       
      }      
      i_debug("tmp_buffer %d ",tmp_buffer.length());
      ret_val = rados_storage->append_to_object(*current_object->get_oid(), tmp_buffer, length) ? 0 : -1; 
    }
    i_debug("append mail (append) return value: %d",ret_val);
    if(ret_val < 0){
      ret_val = -1;
      break;
    }
  }
  // deprecated unused
  current_object->set_write_operation(nullptr);
  current_object->set_completion(nullptr);
  current_object->set_active_op(0);
  
  i_debug("freeing mailbuffer");
  // free mail's buffer cause we don't need it anymore
  librados::bufferlist *mail_buffer = current_object->get_mail_buffer();
  delete mail_buffer;

  return ret_val;
}

int rbox_save_finish(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  bool zlib_plugin_active = false;

// clean stream if still open
#if DOVECOT_PREREQ(2, 3)    
    int ret = 0;
    //#355 we need to write zipl trailer even the stream is empty!
    if (r_ctx->ctx.data.output != r_ctx->output_stream ) {
      /* e.g. zlib plugin had changed this. make sure we
             successfully write the trailer. */
      i_debug("check state2 %ld",r_ctx->ctx.data.output);
      ret = o_stream_finish(r_ctx->ctx.data.output);
    } else if(r_ctx->ctx.data.output != NULL) {
      //TODO: check for error in the stream before using flush
      /* no plugins - flush the output so far */
      i_debug("no plugins flish the output so far %ld",r_ctx->ctx.data.output);
      ret = o_stream_flush(r_ctx->ctx.data.output);

    }
    if (ret < 0) {

      mail_set_critical(r_ctx->ctx.dest_mail, "write(%s) failed: %s", o_stream_get_name(r_ctx->ctx.data.output),
                        o_stream_get_error(r_ctx->ctx.data.output));
      r_ctx->failed = TRUE;
    }
#else
    if (o_stream_nfinish(r_ctx->ctx.data.output) < 0) {
      mail_storage_set_critical(r_ctx->ctx.transaction->box->storage, "write(%s) failed: %m",
                                o_stream_get_name(r_ctx->ctx.data.output));
      r_ctx->failed = TRUE;
    }
    if (r_ctx->ctx.data.output == NULL) {
      i_assert(r_ctx->failed);
    }
#endif
  if (!r_ctx->failed) {
    
    if (_ctx->data.save_date != (time_t)-1) {
      uint32_t t = _ctx->data.save_date;
      index_mail_cache_add((struct index_mail *)_ctx->dest_mail, MAIL_CACHE_SAVE_DATE, &t, sizeof(t));
    }
    
    if (r_ctx->ctx.data.output != r_ctx->output_stream) {
      /* e.g. zlib plugin had changed this */
      o_stream_ref(r_ctx->output_stream);
      o_stream_destroy(&r_ctx->ctx.data.output);
      r_ctx->ctx.data.output = r_ctx->output_stream;
      zlib_plugin_active = true;
    }

    // reset virtual size
    index_mail_cache_parse_deinit(_ctx->dest_mail, r_ctx->ctx.data.received_date, !r_ctx->failed);
    if (r_ctx->output_stream->offset <= 0) {
      // error mail size is null
      r_ctx->failed = true;
      i_error("ERROR, mailsize is <= 0 ");
    } 
    else {

      if (!zlib_plugin_active) {
        // write \0 to ceph (length()+1) if stream is not binary
        r_ctx->rados_mail->set_mail_size(r_ctx->output_stream->offset);

      } else {
        // binary stream, do not modify the length of stream.
        r_ctx->rados_mail->set_mail_size(r_ctx->output_stream->offset);
      }

      rbox_save_mail_set_metadata(r_ctx, r_ctx->rados_mail);

      librados::ObjectWriteOperation write_op;
      struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;

      r_storage->ms->get_storage()->save_metadata(&write_op, r_ctx->rados_mail);

      int max_object_size = r_storage->s->get_max_object_size();
      i_debug("oid: %s, max_object_size %d mail_size %d",r_ctx->rados_mail->get_oid()->c_str(), max_object_size, r_ctx->rados_mail->get_mail_size() );
      if(max_object_size < r_ctx->rados_mail->get_mail_size()) {
        i_error("configured CEPH Object size %d < then mail size %d ", r_storage->s->get_max_object_size(), r_ctx->rados_mail->get_mail_size() );
        mail_set_critical(r_ctx->ctx.dest_mail, "write(%s) failed: %s", o_stream_get_name(r_ctx->ctx.data.output),"MAX OBJECT SIZE REACHED");      
        r_ctx->failed = true;  
      }else {
        
          time_t save_date = r_ctx->rados_mail->get_rados_save_date();
          write_op.mtime(&save_date);  

          uint32_t config_chunk_size = r_storage->config->get_chunk_size();
          if(config_chunk_size > r_storage->s->get_max_write_size_bytes()){
            config_chunk_size = r_storage->s->get_max_write_size_bytes();
          }

          int ret = save_mail_write_append(r_storage->s,r_ctx->rados_mail, &write_op, config_chunk_size);

          r_ctx->failed = ret < 0;
          i_debug("SAVE_MAIL result: %d", r_ctx->failed);        
      }
      if (r_ctx->failed) {
        i_error("saved mail: %s failed. Metadata_count %ld, mail_size (%d)", r_ctx->rados_mail->get_oid()->c_str(),
                r_ctx->rados_mail->get_metadata()->size(), r_ctx->rados_mail->get_mail_size());
      }else{
        if( r_storage->config->get_object_search_method() == 2){
          // ceph config schalter an oder aus!
          r_storage->s->ceph_index_append(*r_ctx->rados_mail->get_oid());                    
          if( librmb::RadosUtils::object_size_close_to_reach_max(
                                                                 (double)r_storage->s->ceph_index_size(), 
                                                                 (double) r_storage->s->get_max_object_size())
                                                                ) {
            i_warning("ceph index file (%lu) close to exceed max object size(%d) 80%%",r_storage->s->ceph_index_size(), r_storage->s->get_max_object_size());
          }
        }
      }

      if (r_storage->save_log->is_open()) {
        r_storage->save_log->append(
            librmb::RadosSaveLogEntry(*r_ctx->rados_mail->get_oid(), r_storage->s->get_namespace(),
                                      r_storage->s->get_pool_name(), librmb::RadosSaveLogEntry::op_save()));                        
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

  FUNC_END();
}

static int rbox_save_assign_uids(struct rbox_save_context *r_ctx, const ARRAY_TYPE(seq_range) * uids) {
  FUNC_START();

  struct seq_range_iter iter;
  uint32_t uid = -1;

  if (r_ctx->rados_mails.size() > 0) {
    unsigned int n = 0;
    seq_range_array_iter_init(&iter, uids);
    struct rbox_storage *r_storage = (struct rbox_storage *)&r_ctx->mbox->storage->storage;
    RadosMetadata metadata;
    for (std::list<RadosMail *>::iterator it = r_ctx->rados_mails.begin(); it != r_ctx->rados_mails.end(); ++it) {
      r_ctx->rados_mail = *it;
      bool ret = seq_range_array_iter_nth(&iter, n++, &uid);
      i_assert(ret);
      if (r_storage->config->is_mail_attribute(rbox_metadata_key::RBOX_METADATA_MAIL_UID)) {
        metadata.convert(rbox_metadata_key::RBOX_METADATA_MAIL_UID, uid);

        librados::ObjectWriteOperation write_mail_uid;
        write_mail_uid.setxattr(metadata.key.c_str(), metadata.bl);

        if (r_storage->ms->get_storage()->set_metadata(r_ctx->rados_mail, metadata, &write_mail_uid) < 0) {
          return -1;
        }
      }
#if DOVECOT_PREREQ(2, 3)
      if (r_ctx->highest_pop3_uidl_seq == n + 1) {
        index_pop3_uidl_set_max_uid(&r_ctx->mbox->box, r_ctx->trans, uid);
      }
#endif
    }
    i_assert(!seq_range_array_iter_nth(&iter, n, &uid));
  }

  FUNC_END();
  return 0;
}
void rbox_save_update_header_flags(struct rbox_save_context *r_ctx, struct mail_index_view *sync_view, uint32_t ext_id,
                                   unsigned int flags_offset) {
  FUNC_START();

  const void *data = NULL;
  size_t data_size = -1;
  uint8_t old_flags = 0, flags = 0;

  mail_index_get_header_ext(sync_view, ext_id, &data, &data_size);
  if (data == NULL) {
    i_error("mail_index_get_header_ext failed to load extended heder for ext_id : %d", ext_id);
    return;
  }
  if (flags_offset < data_size) {
    old_flags = *((const uint8_t *)data + flags_offset);
  } else {
    /* grow old dbox header */
    mail_index_ext_resize_hdr(r_ctx->trans, ext_id, flags_offset + 1);
  }

  flags = old_flags;
  if (r_ctx->have_pop3_uidls)
    flags |= RBOX_INDEX_HEADER_FLAG_HAVE_POP3_UIDLS;
  if (r_ctx->have_pop3_orders)
    flags |= RBOX_INDEX_HEADER_FLAG_HAVE_POP3_ORDERS;
  if (flags != old_flags) {
    /* flags changed, update them */
    mail_index_update_header_ext(r_ctx->trans, ext_id, flags_offset, &flags, 1);
  }

  FUNC_END();
}
int rbox_transaction_save_commit_pre(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  i_assert(r_ctx->finished);

  if (rbox_sync_begin(r_ctx->mbox, &r_ctx->sync_ctx,
                      static_cast<enum rbox_sync_flags>(RBOX_SYNC_FLAG_FORCE | RBOX_SYNC_FLAG_FSYNC)) < 0) {
    r_ctx->failed = TRUE;
    rbox_transaction_save_rollback(_ctx);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  /* update rbox header flags */
  rbox_save_update_header_flags(r_ctx, r_ctx->sync_ctx->sync_view, r_ctx->mbox->hdr_ext_id,
                                offsetof(struct rbox_index_header, flags));

  /* assign UIDs for new messages */
  const struct mail_index_header *hdr = mail_index_get_header(r_ctx->sync_ctx->sync_view);
  if (hdr == NULL) {
    i_error("mail_index_get_header failed");
    return -1;
  }
  // note dovecot 2.3 is using stashed away uids, this mechanism is not used for now.
  mail_index_append_finish_uids(r_ctx->trans, hdr->next_uid, &_ctx->transaction->changes->saved_uids);

  if (rbox_save_assign_uids(r_ctx, &_ctx->transaction->changes->saved_uids) < 0) {
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
  _ctx->transaction->changes->uid_validity = hdr->uid_validity;

  FUNC_END();
  return 0;
}

void rbox_transaction_save_commit_post(struct mail_save_context *_ctx,
                                       struct mail_index_transaction_commit_result *result) {
  FUNC_START();

  _ctx->transaction = NULL; /* transaction is already freed */

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;

  mail_index_sync_set_commit_result(r_ctx->sync_ctx->index_sync_ctx, result);

  if (rbox_sync_finish(&r_ctx->sync_ctx, TRUE) < 0) {
    r_ctx->failed = TRUE;    
  }
  rbox_transaction_save_rollback(_ctx);

  FUNC_END();
}

void clean_up_mail_object_list(struct rbox_save_context *r_ctx, struct rbox_storage *r_storage) {
  FUNC_START();

  for (std::list<RadosMail *>::iterator it = r_ctx->rados_mails.begin(); it != r_ctx->rados_mails.end(); ++it) {
    r_storage->s->free_rados_mail(*it);
    *it = nullptr;
  }
  r_ctx->rados_mails.clear();

  FUNC_END();
}

void rbox_transaction_save_rollback(struct mail_save_context *_ctx) {
  FUNC_START();

  struct rbox_save_context *r_ctx = (struct rbox_save_context *)_ctx;
  librmb::RadosStorage *storage = ((struct rbox_storage *)&r_ctx->mbox->storage->storage)->s;

  if (!r_ctx->finished) {
    rbox_save_cancel(&r_ctx->ctx);
    clean_up_write_finish(_ctx);
  }

  if (r_ctx->sync_ctx != NULL)
    (void)rbox_sync_finish(&r_ctx->sync_ctx, FALSE);

  // empty em.
  guid_128_empty(r_ctx->mail_guid);
  guid_128_empty(r_ctx->mail_oid);
  if (_ctx->dest_mail != NULL && r_ctx->dest_mail_allocated == TRUE) {
    mail_free(&_ctx->dest_mail);
  }

  if (r_ctx->failed) {
    // delete index entry and delete object if it exist
    if (!r_ctx->copying || !_ctx->moving) {
      clean_up_failed(r_ctx, true);
    }
  }
  clean_up_mail_object_list(r_ctx, (struct rbox_storage *)&r_ctx->mbox->storage->storage);

  r_ctx->rados_mail = nullptr;

  delete r_ctx;

  FUNC_END();
}
