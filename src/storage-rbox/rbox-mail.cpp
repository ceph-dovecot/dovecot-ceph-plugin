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

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <sys/resource.h>
#include <sys/time.h>

#include <map>
#include <string>
#include <iostream>
#include <vector>

extern "C" {

#include "dovecot-all.h"

#include "istream.h"
#include "ostream.h"
#include "index-mail.h"
#include "debug-helper.h"
#include "limits.h"
}

#include "rados-mail-object.h"
#include "rbox-storage.hpp"
#include "../librmb/rados-storage-impl.h"

#include "rbox-mail.h"

using librmb::RadosMailObject;
using librmb::rbox_metadata_key;

static void rbox_mail_set_expunged(struct rbox_mail *mail) {
  struct mail *_mail = &mail->imail.mail.mail;

  mail_index_refresh(_mail->box->index);
  if (mail_index_is_expunged(_mail->transaction->view, _mail->seq)) {
    mail_set_expunged(_mail);
    return;
  }

  mail_storage_set_critical(_mail->box->storage, "rbox %s: Unexpectedly lost uid=%u", mailbox_get_path(_mail->box),
                            _mail->uid);
  rbox_set_mailbox_corrupted(_mail->box);
}

int rbox_get_index_record(struct mail *_mail) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)_mail->transaction->box;

  i_debug("last_seq=%" PRIu32 ", mail_seq=%" PRIu32 ", ext_id=%" PRIu32 ", uid=%" PRIu32 ", old_oid=%s",
          rmail->last_seq, _mail->seq, rbox->ext_id, _mail->uid, rmail->mail_object->get_oid().c_str());

  if (rmail->last_seq != _mail->seq) {
    const struct obox_mail_index_record *obox_rec;
    const void *rec_data;
    mail_index_lookup_ext(_mail->transaction->view, _mail->seq, rbox->ext_id, &rec_data, NULL);
    obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);

    if (obox_rec == nullptr) {
      i_debug("no index entry for %d, ext_id=%d ,mail_object->oid='%s'", _mail->seq, rbox->ext_id,
              rmail->mail_object->get_oid().c_str());
      /* lost for some reason, give up */
      FUNC_END_RET("ret == -1");
      return -1;
    }

    memcpy(rmail->index_guid, obox_rec->guid, sizeof(obox_rec->guid));
    memcpy(rmail->index_oid, obox_rec->oid, sizeof(obox_rec->oid));

    rmail->mail_object->set_oid(guid_128_to_string(rmail->index_oid));
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

  mail->imail.mail.pool = pool;
  index_mail_init(&mail->imail, t, wanted_fields, wanted_headers);

  FUNC_END();
  return &mail->imail.mail.mail;
}

static int rbox_mail_metadata_get(struct rbox_mail *rmail, enum rbox_metadata_key key, char **value_r) {
  struct mail *mail = (struct mail *)rmail;
  struct rbox_storage *r_storage = (struct rbox_storage *)mail->box->storage;
  int ret = -1;
  if (rbox_open_rados_connection(mail->box) < 0) {
    i_debug("ERROR, cannot open rados connection (rbox_mail_metadata_get)");
    return -1;
  }

  ret = r_storage->s->load_metadata(rmail->mail_object);
  if (ret < 0) {
    i_debug("ret == -1; cannot get x_attr from object %s", rmail->mail_object->get_oid().c_str());
    return ret;
  }

  std::string value = rmail->mail_object->get_metadata(key);
  if (!value.empty()) {
    *value_r = i_strdup(value.c_str());
  }
  return 0;
}

static int rbox_mail_get_received_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct index_mail_data *data = &mail->data;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  char *value = NULL;
  int ret = 0;
  if (index_mail_get_received_date(_mail, date_r) == 0) {
    FUNC_END_RET("ret == 0");
    return 0;
  }

  ret = rbox_mail_metadata_get(rmail, rbox_metadata_key::RBOX_METADATA_RECEIVED_TIME, &value);
  if (ret < 0) {
    if (ret == -ENOENT) {
      rbox_mail_set_expunged(rmail);
      return -1;
    } else {
      FUNC_END_RET("ret == -1; cannot stat object to get received date and object size");
      return -1;
    }
  }

  if (value == NULL)
    return -1;

  data->received_date = static_cast<time_t>(std::stol(value));

  *date_r = data->received_date;
  i_free(value);

  i_debug("received date = %s", ctime(date_r));
  FUNC_END();
  return 0;
}

static int rbox_mail_get_save_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct index_mail_data *data = &mail->data;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;

  if (index_mail_get_save_date(_mail, date_r) == 0) {
    i_debug("save date = %s", ctime(date_r));
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rbox_open_rados_connection(_mail->box) < 0) {
    FUNC_END_RET("ret == -1;  connection to rados failed");
    return -1;
  }
  uint64_t object_size = 0;
  time_t save_date_rados = 0;

  int ret_val = (r_storage->s)->stat_mail(rmail->mail_object->get_oid(), &object_size, &save_date_rados);
  if (ret_val < 0) {
    if (ret_val == -ENOENT) {
      rbox_mail_set_expunged(rmail);
      return -1;
    } else {
      FUNC_END_RET("ret == -1; cannot stat object to get received date and object size");
      return -1;
    }
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
  *size_r = -1;

  bool ret = index_mail_get_cached_virtual_size(&rmail->imail, size_r);
  if (ret && *size_r > 0) {
    return 0;
  }

  if (rbox_mail_metadata_get(rmail, rbox_metadata_key::RBOX_METADATA_VIRTUAL_SIZE, &value) < 0) {
    value = NULL;
  }

  if (value == NULL)
    return index_mail_get_virtual_size(_mail, size_r);

  data->virtual_size = std::stol(value);

  *size_r = data->virtual_size;
  return 0;
}

static int rbox_mail_get_physical_size(struct mail *_mail, uoff_t *size_r) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct index_mail_data *data = &rmail->imail.data;

  char *value = NULL;
  if (index_mail_get_physical_size(_mail, size_r) == 0) {
    i_debug("get_physical_size from index(oid=%s, uid=%d, size=%lu", rmail->mail_object->get_oid().c_str(), _mail->uid,
            *size_r);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rbox_mail_metadata_get(rmail, rbox_metadata_key::RBOX_METADATA_PHYSICAL_SIZE, &value) < 0) {
    value = NULL;
    FUNC_END_RET("ret == -1; rados_read_metadata ");
    return -1;
  }

  if (value == NULL) {
    return -1;
  }

  data->physical_size = std::stol(value);
  *size_r = data->physical_size;

  FUNC_END();
  return 0;
}

static int get_mail_stream(struct rbox_mail *mail, char *buffer, uint64_t physical_size, struct istream **stream_r) {
  struct mail_private *pmail = &mail->imail.mail;
  struct istream *input;  // = *stream_r;
  int ret = 0;

  input = i_stream_create_from_data(buffer, physical_size);
  i_stream_set_max_buffer_size(input, physical_size);
  i_stream_seek(input, 0);

  *stream_r = i_stream_create_limit(input, physical_size);

  if (pmail->v.istream_opened != NULL) {
    if (pmail->v.istream_opened(&pmail->mail, stream_r) < 0)
      ret = -1;
  }
  i_stream_unref(&input);
  return ret;
}

static int rbox_mail_get_stream(struct mail *_mail, bool get_body ATTR_UNUSED, struct message_size *hdr_size,
                                struct message_size *body_size, struct istream **stream_r) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct istream *input;  // = *stream_r;

  struct index_mail_data *data = &rmail->imail.data;

  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
  int ret = 0;

  i_debug("rbox_mail_get_stream(oid=%s, uid=%d)", rmail->mail_object->get_oid().c_str(), _mail->uid);
  uint64_t size_r = 0;

  if (data->stream == NULL /* && rmail->mail_buffer == NULL*/) {
    if (rbox_open_rados_connection(_mail->box) < 0) {
      FUNC_END_RET("ret == -1;  connection to rados failed");
      return -1;
    }

    if (rmail->mail_buffer != NULL) {
      i_free(rmail->mail_buffer);
    }

    _mail->transaction->stats.open_lookup_count++;

    librados::bufferlist mail_data_bl;
    size_r = r_storage->s->read_mail(&mail_data_bl, rmail->mail_object->get_oid());
    if (size_r <= 0) {
      if (size_r == (uint64_t)-ENOENT) {
        rbox_mail_set_expunged(rmail);
        return -1;
      } else {
        i_debug("error code: %lu", size_r);
        FUNC_END_RET("ret == -1");
        return -1;
      }
    } else {
      rmail->mail_buffer = p_new(default_pool, char, size_r + 1);
      if (rmail->mail_buffer == NULL) {
        FUNC_END_RET("ret == -1; out of memory");
        return -1;
      }
      memcpy(rmail->mail_buffer, mail_data_bl.to_str().c_str(), size_r + 1);
    }
    // ret = r_storage->s->read_mail(rmail->mail_object->get_oid(), &size_r, rmail->mail_buffer);
    get_mail_stream(rmail, rmail->mail_buffer, size_r, &input);

    uoff_t size_decompressed = -1;
    i_stream_get_size(input, TRUE, &size_decompressed);

    data->stream = input;
    index_mail_set_read_buffer_size(_mail, input);
  }
  ret = index_mail_init_stream(&rmail->imail, hdr_size, body_size, stream_r);

  FUNC_END();
  return ret;
}

static int rbox_get_cached_metadata(struct rbox_mail *mail, enum rbox_metadata_key key,
                                    enum index_cache_field cache_field, const char **value_r) {
  struct index_mail *imail = &mail->imail;
  struct index_mailbox_context *ibox =
      reinterpret_cast<index_mailbox_context *>(INDEX_STORAGE_CONTEXT(imail->mail.mail.box));
  char *value;
  string_t *str;
  uint32_t order;

  str = str_new(imail->mail.data_pool, 64);
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
  int ret;

  /* keep the UIDL in cache file, otherwise POP3 would open all
     mail files and read the metadata. same for GUIDs if they're
     used. */
  switch (field) {
    case MAIL_FETCH_GUID:
      return rbox_get_cached_metadata(mail, rbox_metadata_key::RBOX_METADATA_GUID, MAIL_CACHE_GUID, value_r);
    case MAIL_FETCH_UIDL_BACKEND:
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_INDEX_POP3_UIDL_H
      if (!index_pop3_uidl_can_exist(_mail)) {
        *value_r = "";
        return 0;
      }
#endif

      ret = rbox_get_cached_metadata(mail, rbox_metadata_key::RBOX_METADATA_POP3_UIDL, MAIL_CACHE_POP3_UIDL, value_r);

#ifdef DOVECOT_CEPH_PLUGINS_HAVE_INDEX_POP3_UIDL_H
      if (ret == 0) {
        index_pop3_uidl_update_exists(&mail->imail.mail.mail, (*value_r)[0] != '\0');
      }
#endif
      return ret;
    case MAIL_FETCH_POP3_ORDER:
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_INDEX_POP3_UIDL_H
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
  if (rmail_->mail_buffer != NULL) {
    i_free(rmail_->mail_buffer);
    rmail_->mail_buffer = NULL;
  }
  if (rmail_->mail_object != NULL) {
    delete rmail_->mail_object;
    rmail_->mail_object = NULL;
  }

  index_mail_close(_mail);
}

static void rbox_index_mail_set_seq(struct mail *_mail, uint32_t seq, bool saving) {
  struct rbox_mail *rmail_ = (struct rbox_mail *)_mail;
  // close mail and set sequence
  index_mail_set_seq(_mail, seq, saving);

  if (rmail_->mail_object == NULL) {
    struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
    rmail_->mail_object = r_storage->s->create_mail_object();
    rbox_get_index_record(_mail);
  }
}

/*ebd if old version */
// rbox_mail_free,
struct mail_vfuncs rbox_mail_vfuncs = {
    rbox_mail_close, index_mail_free, rbox_index_mail_set_seq, index_mail_set_uid, index_mail_set_uid_cache_updates,
    index_mail_prefetch, index_mail_precache, index_mail_add_temp_wanted_fields,

    index_mail_get_flags, index_mail_get_keywords, index_mail_get_keyword_indexes, index_mail_get_modseq,
    index_mail_get_pvt_modseq, index_mail_get_parts, index_mail_get_date, rbox_mail_get_received_date,
    rbox_mail_get_save_date, rbox_mail_get_virtual_size, rbox_mail_get_physical_size, index_mail_get_first_header,
    index_mail_get_headers, index_mail_get_header_stream, rbox_mail_get_stream, index_mail_get_binary_stream,
    rbox_mail_get_special,
#if DOVECOT_PREREQ(2, 3)
    index_mail_get_backend_mail,
#else
    index_mail_get_real_mail,
#endif
    index_mail_update_flags, index_mail_update_keywords, index_mail_update_modseq, index_mail_update_pvt_modseq, NULL,
    index_mail_expunge, index_mail_set_cache_corrupted, index_mail_opened,
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_INDEX_MAIL_SET_CACHE_CORRUPTED_REASON
    index_mail_set_cache_corrupted_reason
#endif
};
