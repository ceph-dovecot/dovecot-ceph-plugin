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

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <map>
#include <string>
#include <iostream>

extern "C" {

#include "dovecot-all.h"

#include "istream.h"
#include "ostream.h"
#include "index-mail.h"
#include "debug-helper.h"
#include "limits.h"
#include "macros.h"
#if DOVECOT_PREREQ(2, 3)
#include "index-pop3-uidl.h"
#endif
}

#include "../librmb/rados-mail.h"
#include "rbox-storage.hpp"
#include "../librmb/rados-storage-impl.h"
#include "istream-bufferlist.h"
#include "rbox-mail.h"
#include "rados-util.h"

using librmb::RadosMail;
using librmb::rbox_metadata_key;

void rbox_mail_set_expunged(struct rbox_mail *mail) {
  FUNC_START();
  // only set mail to expunge. see #222 rbox_set_expunge => index rebuild!
  mail_set_expunged((struct mail *)mail);
  FUNC_END();
}

int rbox_get_index_record(struct mail *_mail) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)_mail->transaction->box;

  if (rmail->last_seq != _mail->seq) {
    const void *rec_data = NULL;
    mail_index_lookup_ext(_mail->transaction->view, _mail->seq, rbox->ext_id, &rec_data, NULL);
    if (rec_data == NULL) {
      i_error("error mail_index_lookup_ext for mail_seq (%d), ext_id(%d)", _mail->seq, rbox->ext_id);
      /* lost for some reason, give up */
      FUNC_END_RET("ret == -1");
      return -1;
    }
    const struct obox_mail_index_record *obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);
    memcpy(rmail->index_guid, obox_rec->guid, sizeof(obox_rec->guid));
    memcpy(rmail->index_oid, obox_rec->oid, sizeof(obox_rec->oid));

    rmail->rados_mail->set_oid(guid_128_to_string(rmail->index_oid));
    rmail->last_seq = _mail->seq;
  }
  FUNC_END();
  return 0;
}

struct mail *rbox_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                             struct mailbox_header_lookup_ctx *wanted_headers) {
  FUNC_START();
  struct rbox_mail *mail;

  pool_t pool = pool_alloconly_create("mail", 2048);
  mail = p_new(pool, struct rbox_mail, 1);
  i_zero(mail);

#ifdef HAVE_INDEX_MAIL_INIT_OLD_SIGNATURE
  mail->imail.mail.pool = pool;
  index_mail_init(&mail->imail, t, wanted_fields, wanted_headers);
#else
  index_mail_init(&mail->imail, t, wanted_fields, wanted_headers, pool, NULL);
#endif

  FUNC_END();
  return &mail->imail.mail.mail;
}

static int rbox_mail_metadata_get(struct rbox_mail *rmail, enum rbox_metadata_key key, char **value_r) {
  FUNC_START();
  struct mail *mail = (struct mail *)rmail;
  struct rbox_storage *r_storage = (struct rbox_storage *)mail->box->storage;

  *value_r = NULL;

  enum mail_flags flags = index_mail_get_flags(mail);
  bool alt_storage = is_alternate_storage_set(flags) && is_alternate_pool_valid(mail->box);
  if (rbox_open_rados_connection(mail->box, alt_storage) < 0) {
    i_error("Cannot open rados connection (rbox_mail_metadata_get)");
    FUNC_END();
    return -1;
  }
  
  // update metadata storage io_ctx and load metadata
  if (alt_storage) {
    r_storage->ms->get_storage()->set_io_ctx(&r_storage->alt->get_io_ctx());
  } else {
    r_storage->ms->get_storage()->set_io_ctx(&r_storage->s->get_io_ctx());
  }

  /*#283: virtual mailbox needs this (different initialisation path)*/
  if (rmail->rados_mail == nullptr) {
    // make sure that mail_object is initialized,
    // else create and load guid from index.
    rmail->rados_mail = r_storage->s->alloc_rados_mail();
    if (rbox_get_index_record(mail) < 0) {
      i_error("Error rbox_get_index_record uid(%d)", mail->uid);
      FUNC_END();
      return -1;
    }
  }
  
  int ret_load_metadata = r_storage->ms->get_storage()->load_metadata(rmail->rados_mail);
  if (ret_load_metadata < 0) {
    std::string metadata_key = librmb::rbox_metadata_key_to_char(key);
    if (ret_load_metadata == -ENOENT) {
      i_warning("Errorcode: %d cannot get x_attr(%s,%c) from object %s, process %d", ret_load_metadata,
                metadata_key.c_str(), key, rmail->rados_mail->get_oid()->c_str(), getpid());
      rbox_mail_set_expunged(rmail);
    } else {    
      i_error("Errorcode: %d cannot get x_attr(%s,%c) from object %s, process %d", ret_load_metadata,
              metadata_key.c_str(), key, rmail->rados_mail != NULL ? rmail->rados_mail->get_oid()->c_str() : " no oid", getpid());
    }
    FUNC_END();
    return -1;
  }

  // we need to copy the pointer. Because dovecots memory mgmnt will free it!
  char *val = NULL;

  librmb::RadosUtils::get_metadata(key, rmail->rados_mail->get_metadata(), &val);
  if (val != NULL) {
    *value_r = i_strdup(val);
  } else {
    return -1;
  }
  FUNC_END();
  return 0;
}

static int rbox_mail_get_received_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct index_mail_data *data = &rmail->imail.data;

  char *value = NULL;
  bool free_value = true;
  int ret = 0;

  if (index_mail_get_received_date(_mail, date_r) == 0) {
    FUNC_END_RET("ret == 0");
    return ret;
  }
  // in case we already read the metadata this gives us the value
  // void get_metadata(rbox_metadata_key key, std::string* value) {

  if (rmail->rados_mail == nullptr) {
    // make sure that mail_object is initialized,
    // else create and load guid from index.
    struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
    rmail->rados_mail = r_storage->s->alloc_rados_mail();
    if (rbox_get_index_record(_mail) < 0) {
      i_error("Error rbox_get_index uid(%d)", _mail->uid);
      FUNC_END();
      return -1;
    }
  }
  librmb::RadosUtils::get_metadata(rbox_metadata_key::RBOX_METADATA_RECEIVED_TIME, rmail->rados_mail->get_metadata(),
                                   &value);

  if (value == NULL) {
    ret = rbox_mail_metadata_get(rmail, rbox_metadata_key::RBOX_METADATA_RECEIVED_TIME, &value);
    if (ret < 0) {
      if (ret != -ENOENT) {
        // in rbox_mail_metadata_get mail has already been set as expunged!
        FUNC_END_RET("ret == -1; cannot get received date");
      }
      return -1;
    }

    if (value == NULL) {
      // file exists but receive date is unkown, due to missing index entry and missing
      // rados xattribute, as in sdbox this is not necessarily a error so return 0;
      i_error("receive_date for object(%s) is not in index and not in xattribues!",
              rmail->rados_mail->get_oid()->c_str());
      return -1;
    }
  } else {
    // value is ptr to bufferlist.buf
    free_value = false;
  }
  try {
    data->received_date = static_cast<time_t>(std::stol(value));
    *date_r = data->received_date;
  } catch (const std::invalid_argument &e) {
    std::string oid = rmail->rados_mail != nullptr ? *rmail->rados_mail->get_oid() : "";
    i_error("invalid value (invalid argument) for received_date(%s), mail_id(%d), mail_oid(%s)", value, _mail->uid,
            oid.c_str());
    ret = -1;
  } catch (const std::out_of_range &e) {
    std::string oid = rmail->rados_mail != nullptr ? *rmail->rados_mail->get_oid() : "";
    i_error("invalid value (out of range) for received_date(%s), mail_id(%d), mail_oid(%s)", value, _mail->uid,
            oid.c_str());
    ret = -1;
  }
  if (value != NULL && free_value) {
    i_free(value);
  }
  FUNC_END();
  return ret;
}

static int rbox_mail_get_save_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct index_mail_data *data = &rmail->imail.data;
  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
  uint64_t object_size = 0;
  time_t save_date_rados = 0;
  enum mail_flags flags = index_mail_get_flags(_mail);
  bool alt_storage = is_alternate_storage_set(flags) && is_alternate_pool_valid(_mail->box);

  if (index_mail_get_save_date(_mail, date_r) == 0) {
    FUNC_END_RET("ret == 0");
    return 0;
  }
  
  if (rmail->rados_mail == nullptr) {
    // make sure that mail_object is initialized,
    // else create and load guid from index.
    struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
    rmail->rados_mail = r_storage->s->alloc_rados_mail();
    if (rbox_get_index_record(_mail) < 0) {
      i_error("Error rbox_get_index uid(%d)", _mail->uid);
      FUNC_END();
      return -1;
    }
  }

  if (rmail->rados_mail->get_rados_save_date() != -1) {
    *date_r = data->save_date = rmail->rados_mail->get_rados_save_date();
    return 0;
  }

  if (rbox_open_rados_connection(_mail->box, alt_storage) < 0) {
    FUNC_END_RET("ret == -1;  connection to rados failed");
    return -1;
  }

  librmb::RadosStorage *rados_storage = alt_storage ? r_storage->alt : r_storage->s;
  int ret_val = rados_storage->stat_mail(*rmail->rados_mail->get_oid(), &object_size, &save_date_rados);
  if (ret_val < 0) {
    if (ret_val != -ENOENT) {
      FUNC_END_RET("ret == -1; cannot stat object to get received date and object size");
      mail_set_expunged(_mail);
    }
    return -1;
  }
  // check if this is null
  *date_r = data->save_date = save_date_rados;

  FUNC_END();
  return 0;
}

int rbox_mail_get_virtual_size(struct mail *_mail, uoff_t *size_r) {
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct index_mail_data *data = &rmail->imail.data;
  char *value = NULL;
  bool free_value = true;
  *size_r = -1;

  if (index_mail_get_virtual_size(_mail, size_r) == 0) {
    return 0;
  }

  if (index_mail_get_cached_virtual_size(&rmail->imail, size_r) && *size_r > 0) {
    return 0;
  }
  if (rmail->rados_mail == nullptr) {
    // Mail already deleted
    FUNC_END_RET("ret == -1; mail_object == nullptr ");
    return -1;
  }
  librmb::RadosUtils::get_metadata(rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE, rmail->rados_mail->get_metadata(),
                                   &value);

  if (value == NULL) {
    if (rbox_mail_metadata_get(rmail, rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE, &value) < 0) {
      value = NULL;
    }

    if (value == NULL) {
      FUNC_END_RET("ret == -1; mail_object, no x-attribute ");
      return -1;
    }
  } else {
    // value is ptr to bufferlist.buf
    free_value = false;
  }

  int ret = 0;
  try {
    data->virtual_size = std::stol(value);
    *size_r = data->virtual_size;
  } catch (const std::invalid_argument &e) {
    std::string oid = rmail->rados_mail != nullptr ? *rmail->rados_mail->get_oid() : "";
    i_error("invalid value (invalid argument) for virtual_size(%s), mail_id(%d), mail_oid(%s)", value, _mail->uid,
            oid.c_str());
    ret = -1;
  } catch (const std::out_of_range &e) {
    std::string oid = rmail->rados_mail != nullptr ? *rmail->rados_mail->get_oid() : "";
    i_error("invalid value (out of range) for virtual_size(%s), mail_id(%d), mail_oid(%s)", value, _mail->uid,
            oid.c_str());
    ret = -1;
  }
  librmb::RadosMetadata metadata_phy(rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE, data->virtual_size);
  rmail->rados_mail->add_metadata(metadata_phy);

  if (value != NULL && free_value) {
    i_free(value);
  }
  return ret;
}

static int rbox_mail_get_physical_size(struct mail *_mail, uoff_t *size_r) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct index_mail_data *data = &rmail->imail.data;
  char *value = NULL;
  int ret = 0;
  bool free_value = true;

  if (index_mail_get_physical_size(_mail, size_r) == 0) {
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rmail->rados_mail == nullptr) {
    // Mail already deleted
    FUNC_END_RET("ret == -1; mail_object == nullptr ");
    return -1;
  }

  librmb::RadosUtils::get_metadata(rbox_metadata_key::RBOX_METADATA_PHYSICAL_SIZE, rmail->rados_mail->get_metadata(),
                                   &value);

  if (value == NULL) {
    if (rbox_mail_metadata_get(rmail, rbox_metadata_key::RBOX_METADATA_PHYSICAL_SIZE, &value) < 0) {
      value = NULL;
      FUNC_END_RET("ret == -1; rados_read_metadata ");
      return -1;
    }
  } else {
    // value is ptr to bufferlist.buf
    free_value = false;
  }
  try {
    *size_r = data->physical_size = std::stol(value);
  } catch (const std::invalid_argument &e) {
    ret = -1;
    std::string oid = rmail->rados_mail != nullptr ? *rmail->rados_mail->get_oid() : "";
    i_error("invalid value (invalid_argument) for physical_size(%s), mail_id(%d), mail_oid(%s)", value, _mail->uid,
            oid.c_str());
  } catch (const std::out_of_range &e) {
    ret = -1;
    std::string oid = rmail->rados_mail != nullptr ? *rmail->rados_mail->get_oid() : "";

    i_error("invalid value (out of range) for physical_size(%s), mail_id(%d), mail_oid(%s)", value, _mail->uid,
            oid.c_str());
  }
  if (value != NULL && free_value) {
    i_free(value);
  }

  FUNC_END();
  return ret;
}

static int get_mail_stream(struct rbox_mail *mail, librados::bufferlist *buffer, const size_t physical_size,
                           struct istream **stream_r) {
  struct mail_private *pmail = &mail->imail.mail;
  int ret = 0;

  struct istream *input = i_stream_create_from_bufferlist(buffer, physical_size);
  i_stream_seek(input, 0);

  *stream_r = input;
  if (pmail->v.istream_opened != NULL) {
    if (pmail->v.istream_opened(&pmail->mail, stream_r) < 0) {
      i_stream_unref(&input);  // free it.
      ret = -1;
    }
  }

  return ret;
}

static int rbox_mail_get_stream(struct mail *_mail, bool get_body ATTR_UNUSED, struct message_size *hdr_size,
                                struct message_size *body_size, struct istream **stream_r) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct istream *input = NULL;
  struct index_mail_data *data = &rmail->imail.data;
  int ret = -1;
  enum mail_flags flags = index_mail_get_flags(_mail);
  bool alt_storage = is_alternate_storage_set(flags) && is_alternate_pool_valid(_mail->box);
  
  if (data->stream == NULL) {
    if (rbox_open_rados_connection(_mail->box, alt_storage) < 0) {
      FUNC_END_RET("ret == -1;  connection to rados failed");
      return -1;
    }
    
    librmb::RadosStorage *rados_storage = alt_storage ? ((struct rbox_storage *)_mail->box->storage)->alt
                                                      : ((struct rbox_storage *)_mail->box->storage)->s;
    if (alt_storage) {
      rados_storage->set_namespace(rados_storage->get_namespace());
    }

    /* Pop3 and virtual box needs this. it looks like rbox_index_mail_set_seq is not called. */
    if (rmail->rados_mail == nullptr) {
      // make sure that mail_object is initialized,
      // else create and load guid from index.
      rmail->rados_mail = rados_storage->alloc_rados_mail();
      if (rbox_get_index_record(_mail) < 0) {
        i_error("Error rbox_get_index uid(%d)", _mail->uid);
        FUNC_END();
        return -1;
      }
    }
    // create mail buffer!
    rmail->rados_mail->set_mail_buffer(new librados::bufferlist());

    uint64_t psize;
    time_t save_date;

    int stat_err = 0;
    int read_err = 0;

    librados::ObjectReadOperation *read_mail = new librados::ObjectReadOperation();
    read_mail->read(0, INT_MAX, rmail->rados_mail->get_mail_buffer(), &read_err);
    read_mail->stat(&psize, &save_date, &stat_err);

    librados::AioCompletion *completion = librados::Rados::aio_create_completion();
    ret = rados_storage->get_io_ctx().aio_operate(*rmail->rados_mail->get_oid(), completion, read_mail,
                                                  rmail->rados_mail->get_mail_buffer());
    completion->wait_for_complete_and_cb();
    ret = completion->get_return_value();
    completion->release();
    delete read_mail;

    if (ret < 0) {
      if (ret == -ENOENT) {
        i_warning("Mail not found. %s, ns='%s', process %d, alt_storage(%d) -> marking mail as expunged!",
                  rmail->rados_mail->get_oid()->c_str(), rados_storage->get_namespace().c_str(), getpid(), alt_storage);
        rbox_mail_set_expunged(rmail);
        FUNC_END_RET("ret == -1");
        delete rmail->rados_mail->get_mail_buffer();
        return -1;
      } else {
        i_error("reading mail return code(%d), oid(%s),namespace(%s), alt_storage(%d)", ret,
                rmail->rados_mail->get_oid()->c_str(), rados_storage->get_namespace().c_str(), alt_storage);
        FUNC_END_RET("ret == -1");
        delete rmail->rados_mail->get_mail_buffer();
        return -1;
      }
    }
    int physical_size = psize;
    rmail->rados_mail->set_mail_size(psize);
    rmail->rados_mail->set_rados_save_date(save_date);

    if (physical_size == 0) {
      i_error(
          "trying to read a mail(%s) with size = 0, namespace(%s), alt_storage(%d) uid(%d), which is currently copied, "
          "moved "
          "or stored, returning with error => "
          "expunging mail ",
          rmail->rados_mail->get_oid()->c_str(), rados_storage->get_namespace().c_str(), alt_storage, _mail->uid);
      FUNC_END_RET("ret == 0");
      rbox_mail_set_expunged(rmail);
      delete rmail->rados_mail->get_mail_buffer();
      return -1;
    } else if (physical_size == INT_MAX) {
      i_error("trying to read a mail with INT_MAX size. (uid=%d,oid=%s,namespace=%s,alt_storage=%d)", _mail->uid,
              rmail->rados_mail->get_oid()->c_str(), rados_storage->get_namespace().c_str(), alt_storage);
      FUNC_END_RET("ret == -1");
      delete rmail->rados_mail->get_mail_buffer();
      return -1;
    }

    if (get_mail_stream(rmail, rmail->rados_mail->get_mail_buffer(), physical_size, &input) < 0) {
      FUNC_END_RET("ret == -1");
      delete rmail->rados_mail->get_mail_buffer();
      return -1;
    }

    data->stream = input;
    index_mail_set_read_buffer_size(_mail, input);
  }
  ret = index_mail_init_stream(&rmail->imail, hdr_size, body_size, stream_r);
  FUNC_END();
  return ret;
}

// guid is saved in the obox header, and should be available when rbox_mail does exist. (rbox_get_index_record)
int rbox_get_guid_metadata(struct rbox_mail *mail, const char **value_r) {
  if (!guid_128_is_empty(mail->index_guid)) {
    struct index_mail *imail = &mail->imail;
    string_t *str = str_new(imail->mail.data_pool, 64);
    str_truncate(str, 0);
    // always provide uuid compact format.
    str_append(str, guid_128_to_uuid_string(mail->index_guid, FORMAT_COMPACT));
    *value_r = str_c(str);
    return 0;
  }
  // lost for some reason, use fallback
  // index is empty. we have to check the object attributes do we have to tell someone that the index is broken?
  if (rbox_mail_metadata_get(mail, rbox_metadata_key::RBOX_METADATA_GUID, value_r) < 0) {
    return -1;
  }

  // restore the index extension header quietly.
  if(guid_128_from_uuid_string(*value_r, mail->index_guid)< 0){  
    i_error("guid_128 xattr_guid string '%s'", *value_r);
    return -1; 
  }
  struct index_mail *imail = &mail->imail;
  struct mail *_mail = (struct mail *)mail;
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)_mail->box;

  i_warning("guid for mail with uid : %d, seq = %d was lost restoring guid.", _mail->uid, _mail->seq);

  /* save the 128bit GUID/OID to index record */
  struct obox_mail_index_record rec;
  memcpy(rec.guid, mail->index_guid, sizeof(mail->index_guid));
  memcpy(rec.oid, mail->index_oid, sizeof(mail->index_oid));
  mail_index_update_ext(_mail->transaction->itrans, imail->mail.mail.seq, mbox->ext_id, &rec, NULL);

  return 0;
}

static int rbox_get_cached_metadata(struct rbox_mail *mail, enum rbox_metadata_key key,
                                    enum index_cache_field cache_field, const char **value_r) {
  struct index_mail *imail = &mail->imail;
  struct index_mailbox_context *ibox =
      reinterpret_cast<index_mailbox_context *>(RBOX_INDEX_STORAGE_CONTEXT(imail->mail.mail.box));

  char *value = NULL;
  unsigned int order = 0;

  string_t *str = str_new(imail->mail.data_pool, 64);
  if (mail_cache_lookup_field(imail->mail.mail.transaction->cache_view, str, imail->mail.mail.seq,
                              ibox->cache_fields[cache_field].idx) > 0) {
    if (cache_field == MAIL_CACHE_POP3_ORDER) {
      i_assert(str_len(str) == sizeof(order));
      memcpy(&order, str_data(str), sizeof(order));
      str_truncate(str, 0);
      if (order != 0) {
        str_printfa(str, "%u", order);
      } else {
        /* order=0 means it doesn't exist. we don't
           want to return "0" though, because then the
           mails get ordered to beginning, while
           nonexistent are supposed to be ordered at
           the end. */
      }
    }
    *value_r = str_c(str);
    return 0;
  }

  if (rbox_mail_metadata_get(mail, key, &value) < 0)
    return -1;

  if (value == NULL) {
    value = i_strdup("");
  }
  if (cache_field != MAIL_CACHE_POP3_ORDER) {
    index_mail_cache_add_idx(imail, ibox->cache_fields[cache_field].idx, value, strlen(value) + 1);
  } else {
    if (str_to_uint(value, &order) < 0)
      order = 0;

    index_mail_cache_add_idx(imail, ibox->cache_fields[cache_field].idx, &order, sizeof(order));
  }

  /* don't return pointer to rbox metadata directly, since it may
     change unexpectedly */
  str_truncate(str, 0);
  str_append(str, value);
  i_free(value);
  *value_r = str_c(str);
  return 0;
}

static int rbox_mail_get_special(struct mail *_mail, enum mail_fetch_field field, const char **value_r) {
  struct rbox_mail *mail = (struct rbox_mail *)_mail;
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)_mail->box;
  int ret = 0;

  *value_r = NULL;
  /* keep the UIDL in cache file, otherwise POP3 would open all
     mail files and read the metadata. same for GUIDs if they're
     used. */
  switch (field) {
    case MAIL_FETCH_GUID:
      ret = rbox_get_cached_metadata(mail, rbox_metadata_key::RBOX_METADATA_GUID, MAIL_CACHE_GUID, value_r); 
      if( ret < 0){
          return rbox_get_guid_metadata(mail, value_r);
      }    
      return ret;
    case MAIL_FETCH_UIDL_BACKEND:
      if (!rbox_header_have_flag(_mail->box, mbox->hdr_ext_id, offsetof(struct rbox_index_header, flags),
                                 RBOX_INDEX_HEADER_FLAG_HAVE_POP3_UIDLS)) {
        *value_r = "";
        return 0;
      }
#if DOVECOT_PREREQ(2, 3)
      if (!index_pop3_uidl_can_exist(_mail)) {
        *value_r = "";
        return 0;
      }
#endif
      ret = rbox_get_cached_metadata(mail, rbox_metadata_key::RBOX_METADATA_POP3_UIDL, MAIL_CACHE_POP3_UIDL, value_r);
#if DOVECOT_PREREQ(2, 3)
      if (ret == 0) {
        index_pop3_uidl_update_exists(&mail->imail.mail.mail, (*value_r)[0] != '\0');
      }
#endif      
      return ret;
    case MAIL_FETCH_POP3_ORDER:
      if (!rbox_header_have_flag(_mail->box, mbox->hdr_ext_id, offsetof(struct rbox_index_header, flags),
                                 RBOX_INDEX_HEADER_FLAG_HAVE_POP3_ORDERS)) {
        *value_r = "";
        return 0;
      }
#if DOVECOT_PREREQ(2, 3)
      if (!index_pop3_uidl_can_exist(_mail)) {
        /* we're assuming that if there's a POP3 order, there's
           also a UIDL */
        *value_r = "";
        return 0;
      }
#endif
      return rbox_get_cached_metadata(mail, rbox_metadata_key::RBOX_METADATA_POP3_ORDER, MAIL_CACHE_POP3_ORDER,
                                      value_r);

    case MAIL_FETCH_FLAGS:
    // although it is possible to save the flags as xattr. we currently load them directly
    // from index.
    case MAIL_FETCH_MESSAGE_PARTS:
    case MAIL_FETCH_STREAM_HEADER:
    case MAIL_FETCH_STREAM_BODY:
    case MAIL_FETCH_DATE:
    case MAIL_FETCH_RECEIVED_DATE:
    case MAIL_FETCH_SAVE_DATE:
    case MAIL_FETCH_PHYSICAL_SIZE:
    case MAIL_FETCH_VIRTUAL_SIZE:
    case MAIL_FETCH_NUL_STATE:
    case MAIL_FETCH_STREAM_BINARY:
    case MAIL_FETCH_IMAP_BODY:
    case MAIL_FETCH_IMAP_BODYSTRUCTURE:
    case MAIL_FETCH_IMAP_ENVELOPE:
    case MAIL_FETCH_FROM_ENVELOPE:
    case MAIL_FETCH_HEADER_MD5:
    case MAIL_FETCH_STORAGE_ID:
    case MAIL_FETCH_MAILBOX_NAME:
    case MAIL_FETCH_SEARCH_RELEVANCY:
    case MAIL_FETCH_REFCOUNT:
    case MAIL_FETCH_BODY_SNIPPET:
    default:
      break;
  }

  return index_mail_get_special(_mail, field, value_r);
}

static void rbox_mail_close(struct mail *_mail) {
  struct rbox_mail *rmail_ = (struct rbox_mail *)_mail;
  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;

  if (rmail_->rados_mail != nullptr) {
    r_storage->s->free_rados_mail(rmail_->rados_mail);
    rmail_->rados_mail = nullptr;
  }

  index_mail_close(_mail);
}

static void rbox_index_mail_set_seq(struct mail *_mail, uint32_t seq, bool saving) {
  struct rbox_mail *rmail_ = (struct rbox_mail *)_mail;

  // close mail and set sequence
  index_mail_set_seq(_mail, seq, saving);

  if (rmail_->rados_mail == nullptr) {
    struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;

    rmail_->rados_mail = r_storage->s->alloc_rados_mail();
    rbox_get_index_record(_mail);
  }
}
/*static void rbox_update_pop3_uidl(struct mail *_mail, const char *uidl) { i_debug("UIDL: %s", uidl); }*/
/*ebd if old version */
// rbox_mail_free,
struct mail_vfuncs rbox_mail_vfuncs = {rbox_mail_close,
                                       index_mail_free,
                                       rbox_index_mail_set_seq,
                                       index_mail_set_uid,
                                       index_mail_set_uid_cache_updates,
                                       index_mail_prefetch,
                                       index_mail_precache,
                                       index_mail_add_temp_wanted_fields,

                                       index_mail_get_flags,
                                       index_mail_get_keywords,
                                       index_mail_get_keyword_indexes,
                                       index_mail_get_modseq,
                                       index_mail_get_pvt_modseq,
                                       index_mail_get_parts,
                                       index_mail_get_date,
                                       rbox_mail_get_received_date,
                                       rbox_mail_get_save_date,
                                       rbox_mail_get_virtual_size,
                                       rbox_mail_get_physical_size,
                                       index_mail_get_first_header,
                                       index_mail_get_headers,
                                       index_mail_get_header_stream,
                                       rbox_mail_get_stream,
                                       index_mail_get_binary_stream,
                                       rbox_mail_get_special,
#if DOVECOT_PREREQ(2, 3)
                                       index_mail_get_backend_mail,
#else
    index_mail_get_real_mail,
#endif
                                       index_mail_update_flags,
                                       index_mail_update_keywords,
                                       index_mail_update_modseq,
                                       index_mail_update_pvt_modseq,
                                       NULL /*rbox_update_pop3_uidl*/,
                                       index_mail_expunge,
                                       index_mail_set_cache_corrupted,
                                       index_mail_opened,
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_INDEX_MAIL_SET_CACHE_CORRUPTED_REASON
                                       index_mail_set_cache_corrupted_reason
#endif
};
