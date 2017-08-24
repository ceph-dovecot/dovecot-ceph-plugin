/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

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

#include "lib.h"
#include "typeof-def.h"

#include "istream.h"
#include "index-mail.h"
#include "ioloop.h"
#include "str.h"

#include "ostream.h"
#include "debug-helper.h"
#ifdef HAVE_INDEX_POP3_UIDL_H
#include "index-pop3-uidl.h"
#endif
}

#include "rados-mail-object.h"
#include "rbox-storage.hpp"
#include "rados-storage.h"

#include "rbox-mail.h"

using namespace librmb;  // NOLINT

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

  i_debug("last_seq %lu, mail_seq %lu, ext_id =  %lu, uid=%lu, old_oid=%s", (unsigned long)rmail->last_seq,
          (unsigned long)_mail->seq, (unsigned long)rbox->ext_id, (unsigned long)_mail->uid,
          rmail->mail_object->get_oid().c_str());

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
  uint64_t obj_size = -1;
  rmail->mail_object->set_object_size(obj_size);
  FUNC_END();
  return 0;
}

struct mail *rbox_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                             struct mailbox_header_lookup_ctx *wanted_headers) {
  FUNC_START();

  pool_t pool = pool_alloconly_create("mail", 2048);
  struct rbox_mail *mail = p_new(pool, struct rbox_mail, 1);
  i_zero(mail);

  mail->imail.mail.pool = pool;
  index_mail_init(&mail->imail, t, wanted_fields, wanted_headers);

  FUNC_END();
  return &mail->imail.mail.mail;
}

static int rbox_mail_metadata_get(struct rbox_mail *rmail, enum rbox_metadata_key key, char **value_r) {
  struct mail *mail = (struct mail *)rmail;
  struct rbox_storage *r_storage = (struct rbox_storage *)mail->box->storage;

  if (rbox_open_rados_connection(mail->box) < 0) {
    i_debug("ERROR, cannot open rados connection (rbox_mail_metadata_get)");
    return -1;
  }

  if (r_storage->s->load_xattr(rmail->mail_object) < 0) {
    i_debug("ret == -1; cannot get x_attr from object %s", rmail->mail_object->get_oid().c_str());
    return -1;
  }

  std::string value = rmail->mail_object->get_xvalue(key);
  if (!value.empty()) {
    *value_r = i_strdup(value.c_str());
    return 0;
  }

  return -1;
}

static int rbox_mail_get_received_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct index_mail_data *data = &mail->data;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  char *value;
  uintmax_t time;

  if (index_mail_get_received_date(_mail, date_r) == 0) {
    // i_debug("received date = %s", ctime(date_r));
    //  debug_print_mail(_mail, "rbox-mail::rbox_mail_get_received_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rbox_mail_metadata_get(rmail, RBOX_METADATA_RECEIVED_TIME, &value) < 0)
    return -1;
  time = 0;
  i_debug("received_date received, %s", value);
  if (value != NULL && str_to_uintmax_hex(value, &time) < 0)
    return -1;

  long ts = std::stol(value);
  i_free(value);
  i_debug("received_date is %lu", ts);
  data->received_date = static_cast<time_t>(ts);
  *date_r = data->received_date;

  i_debug("received date = %s", ctime(date_r));
  debug_print_mail(_mail, "rbox-mail::rbox_mail_get_received_date", NULL);
  debug_print_index_mail_data(data, "rbox-mail::rbox_mail_get_received_date", NULL);
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
    debug_print_mail(_mail, "rbox_mail_get_save_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rbox_open_rados_connection(_mail->box) < 0) {
    FUNC_END_RET("ret == -1;  connection to rados failed");
    return -1;
  }
  uint64_t object_size = 0;
  time_t save_date_rados = 0;
  int ret_val = ((r_storage->s)->get_io_ctx()).stat(rmail->mail_object->get_oid(), &object_size, &save_date_rados);
  if (ret_val < 0) {
    //  i_debug("cannot stat object %s to get received date and object size ", rmail->mail_object->get_oid().c_str());
    if (ret_val == ((-1) * ENOENT)) {
      rbox_mail_set_expunged(rmail);
      return -1;
    } else {
      FUNC_END_RET("ret == -1; cannot stat object to get received date and object size");
      return -1;
    }
  }

  // check if this is null
  *date_r = data->save_date = save_date_rados;

  i_debug("save date = %s", ctime(date_r));
  debug_print_mail(_mail, "rbox-mail::rbox_mail_get_save_date", NULL);
  debug_print_index_mail_data(data, "rbox-mail::rbox_mail_get_save_date", NULL);
  FUNC_END();
  return 0;
}

int rbox_mail_get_virtual_size(struct mail *_mail, uoff_t *size_r) {
  struct rbox_mail *mail = (struct rbox_mail *)_mail;
  struct index_mail_data *data = &mail->imail.data;
  const char *value;
  uintmax_t size;

  if (index_mail_get_cached_virtual_size(&mail->imail, size_r))
    return 0;

  if (rbox_mail_metadata_get(mail, RBOX_METADATA_VIRTUAL_SIZE, &value) < 0)
    return -1;
  if (value == NULL)
    return index_mail_get_virtual_size(_mail, size_r);

  if (str_to_uintmax_hex(value, &size) < 0 || size > (uoff_t)-1)
    return -1;
  data->virtual_size = (uoff_t)size;
  *size_r = data->virtual_size;
  return 0;
}

static int rbox_mail_get_physical_size(struct mail *_mail, uoff_t *size_r) {
  FUNC_START();
  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;

  uint64_t file_size = -1;
  time_t time = 0;

  i_debug("rbox_mail_get_physical_size(oid=%s, uid=%d)", rmail->mail_object->get_oid().c_str(), _mail->uid);

  rbox_get_index_record(_mail);
  if (index_mail_get_physical_size(_mail, size_r) == 0) {
    i_debug("rbox_mail_get_physical_size(oid=%s, uid=%d, size=%lu", rmail->mail_object->get_oid().c_str(), _mail->uid,
            *size_r);
    debug_print_mail(_mail, "rbox-mail::rbox_mail_get_physical_size (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");

    return 0;
  }

  if (rbox_open_rados_connection(_mail->box) < 0) {
    FUNC_END_RET("ret == -1;  connection to rados failed");
    return -1;
  }

  int ret_val = ((r_storage->s)->get_io_ctx()).stat(rmail->mail_object->get_oid(), &file_size, &time);
  if (ret_val < 0) {
    if (ret_val == ((-1) * ENOENT)) {
      rbox_mail_set_expunged(rmail);
      return -1;
    } else {
      i_debug("no_object: rmail->mail_object->get_oid() %s, size %lu, uid=%d", rmail->mail_object->get_oid().c_str(),
              file_size, _mail->uid);
      FUNC_END_RET("ret == -1; rbox_read");
      return -1;
    }
  }
  i_debug("rmail->mail_object->get_oid() %s, size %lu, uid=%d", rmail->mail_object->get_oid().c_str(), file_size,
          _mail->uid);

  *size_r = file_size;
  FUNC_END();
  return file_size > 0 ? 0 : -1;
}

static int rbox_mail_get_stream(struct mail *_mail, bool get_body ATTR_UNUSED, struct message_size *hdr_size,
                                struct message_size *body_size, struct istream **stream_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct istream *input;

  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
  int ret = 0;

  i_debug("rbox_mail_get_stream(oid=%s, uid=%d)", rmail->mail_object->get_oid().c_str(), _mail->uid);

  if (mail->data.stream == NULL) {
    if (rbox_open_rados_connection(_mail->box) < 0) {
      FUNC_END_RET("ret == -1;  connection to rados failed");
      return -1;
    }

    uoff_t size_r = 0;

    if (rbox_mail_get_physical_size(_mail, &size_r) < 0) {
      FUNC_END_RET("ret == -1; get mail size");
      return -1;
    }

    if (rmail->mail_buffer != NULL) {
      i_free(rmail->mail_buffer);
    }

    rmail->mail_buffer = p_new(default_pool, char, size_r);
    if (rmail->mail_buffer == NULL) {
      FUNC_END_RET("ret == -1; out of memory");
      return -1;
    }

    memset(rmail->mail_buffer, '\0', sizeof(char) * size_r);
    _mail->transaction->stats.open_lookup_count++;

    ret = ((r_storage->s)->read_mail(rmail->mail_object->get_oid(), size_r, &rmail->mail_buffer[0]));
    if (ret < 0) {
      if (ret == -ENOENT) {
        rbox_mail_set_expunged(rmail);
        return -1;
      } else {
        i_debug("error code: %d", ret);
        FUNC_END_RET("ret == -1");
        return -1;
      }
    }

    input = i_stream_create_from_data(rmail->mail_buffer, size_r);
    i_stream_set_name(input, RadosMailObject::DATA_BUFFER_NAME.c_str());
    index_mail_set_read_buffer_size(_mail, input);

    if (mail->mail.v.istream_opened != NULL) {
      if (mail->mail.v.istream_opened(_mail, &input) < 0) {
        i_stream_unref(&input);
        debug_print_mail(_mail, "rbox-mail::rbox_mail_get_stream (ret -1, 2)", NULL);
        FUNC_END_RET("ret == -1");
        return -1;
      }
    }
    mail->data.stream = input;
  }

  ret = index_mail_init_stream(mail, hdr_size, body_size, stream_r);
  debug_print_mail(_mail, "rbox-mail::rbox_mail_get_stream", NULL);
  FUNC_END();
  return ret;
}

static int rbox_get_cached_metadata(struct rbox_mail *mail, enum rbox_metadata_key key,
                                    enum index_cache_field cache_field, const char **value_r) {
  struct index_mail *imail = &mail->imail;
  struct index_mailbox_context *ibox = (index_mailbox_context *)INDEX_STORAGE_CONTEXT(imail->mail.mail.box);
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
      return rbox_get_cached_metadata(mail, RBOX_METADATA_GUID, MAIL_CACHE_GUID, value_r);
    case MAIL_FETCH_UIDL_BACKEND:
#ifdef HAVE_INDEX_POP3_UIDL_H
      if (!index_pop3_uidl_can_exist(_mail)) {
        *value_r = "";
        return 0;
      }
#endif

      ret = rbox_get_cached_metadata(mail, RBOX_METADATA_POP3_UIDL, MAIL_CACHE_POP3_UIDL, value_r);

#ifdef HAVE_INDEX_POP3_UIDL_H
      if (ret == 0) {
        index_pop3_uidl_update_exists(&mail->imail.mail.mail, (*value_r)[0] != '\0');
      }
#endif
      return ret;
    case MAIL_FETCH_POP3_ORDER:
#ifdef HAVE_INDEX_POP3_UIDL_H
      if (!index_pop3_uidl_can_exist(_mail)) {
        /* we're assuming that if there's a POP3 order, there's
           also a UIDL */
        *value_r = "";
        return 0;
      }
#endif
      return rbox_get_cached_metadata(mail, RBOX_METADATA_POP3_ORDER, MAIL_CACHE_POP3_ORDER, value_r);

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
    // init new mail object and load oid and uuid from index
    rmail_->mail_object = new RadosMailObject();
    rbox_get_index_record(_mail);
  }
}

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
                                       index_mail_get_real_mail,
                                       index_mail_update_flags,
                                       index_mail_update_keywords,
                                       index_mail_update_modseq,
                                       index_mail_update_pvt_modseq,
                                       NULL,
                                       index_mail_expunge,
                                       index_mail_set_cache_corrupted,
                                       index_mail_opened,
#ifdef HAVE_INDEX_MAIL_SET_CACHE_CORRUPTED_REASON
                                       index_mail_set_cache_corrupted_reason
#endif
};
