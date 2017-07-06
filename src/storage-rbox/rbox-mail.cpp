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

#include "rbox-storage.h"
#include "ostream.h"
#include "debug-helper.h"
}

#include "rados-mail-object.h"
#include "rbox-mail.h"
#include "rbox-storage-struct.h"
#include "rados-storage.h"

using namespace librmb;  // NOLINT

int rbox_get_index_record(struct mail *_mail) {
  FUNC_START();
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)_mail->transaction->box;

  i_debug("last_seq %lu, mail_seq %lu, ext_id =  %lu, uid=%d, old_oid=%s", rmail->last_seq, _mail->seq, rbox->ext_id,
          _mail->uid, rmail->mail_object->get_oid().c_str());

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

  mail->mail_object = new RadosMailObject();

  FUNC_END();

  return &mail->imail.mail.mail;
}

static int rbox_mail_get_metadata(struct mail *_mail) {
  FUNC_START();
  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;

  if (_mail->lookup_abort == MAIL_LOOKUP_ABORT_NOT_IN_CACHE) {
    mail_set_aborted(_mail);
    debug_print_mail(_mail, "rbox-mail::rbox_mail_stat (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }
  _mail->mail_metadata_accessed = TRUE;

  _mail->transaction->stats.stat_lookup_count++;

  rbox_get_index_record(_mail);

  std::map<std::string, librados::bufferlist> attrset;
  int ret = ((r_storage->s)->get_io_ctx()).getxattrs(rmail->mail_object->get_oid(), attrset);
  if (ret < 0) {
    i_debug("rbox_mail_get_metadata: getxattrs failed : oid: %s, ret_val = %d", rmail->mail_object->get_oid().c_str(),
            ret);
    FUNC_END_RET("ret == -1; rbox getxattrs");
    return -1;
  }

  rmail->mail_object->set_state(attrset[RadosMailObject::X_ATTR_STATE].to_str());
  rmail->mail_object->set_version(attrset[RadosMailObject::X_ATTR_VERSION].to_str());

  guid_128_t guid;
  if (guid_128_from_string(attrset[RadosMailObject::X_ATTR_GUID].to_str().c_str(), guid) < 0) {
    FUNC_END_RET("ret == -1; value in X_ATTR_GUID guid invalid");
    return -1;
  }
  rmail->mail_object->set_guid(guid);

  rmail->mail_object->set_pop3_uidl(attrset[RadosMailObject::X_ATTR_POP3_UIDL].to_str());

  if (attrset[RadosMailObject::X_ATTR_POP3_ORDER].length() > 0) {
    int pop3_order = std::stoi(attrset[RadosMailObject::X_ATTR_POP3_ORDER].to_str().c_str());
    rmail->mail_object->set_pop3_order(pop3_order);
  }

  time_t send_date = 0;
  int length = attrset[RadosMailObject::X_ATTR_SAVE_DATE].length();
  if (length <= 0) {
    rmail->mail_object->set_save_date(0);
  } else {
    long ts = std::stol(attrset[RadosMailObject::X_ATTR_SAVE_DATE].to_str().c_str());
    rmail->mail_object->set_save_date(static_cast<time_t>(ts));
  }
  uint64_t object_size = 0;
  time_t received_date_rados = 0;
  if (((r_storage->s)->get_io_ctx()).stat(rmail->mail_object->get_oid(), &object_size, &received_date_rados) < 0) {
    i_debug("cannot stat object %s to get received date and object size ", rmail->mail_object->get_oid().c_str());
    FUNC_END_RET("ret == -1; cannot stat object to get received date and object size");
    return -1;
  }
  rmail->mail_object->set_received_date(received_date_rados);
  rmail->mail_object->set_object_size(object_size);

  FUNC_END();
  return 0;
}

static int rbox_mail_get_received_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct index_mail_data *data = &mail->data;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;

  if (index_mail_get_received_date(_mail, date_r) == 0) {
    i_debug("received date = %s", ctime(date_r));
    debug_print_mail(_mail, "rbox-mail::rbox_mail_get_received_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rbox_mail_get_metadata(_mail) < 0) {
    debug_print_mail(_mail, "rbox_mail_get_received_date: rbox_mail_get_metadata failed (ret -1, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  data->received_date = rmail->mail_object->get_received_date();
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

  if (index_mail_get_save_date(_mail, date_r) == 0) {
    i_debug("save date = %s", ctime(date_r));
    debug_print_mail(_mail, "rbox_mail_get_save_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rbox_mail_get_metadata(_mail) < 0) {
    debug_print_mail(_mail, "rbox_mail_get_save_date: rbox_mail_get_metadata failed (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  data->save_date = rmail->mail_object->get_save_date();
  *date_r = data->save_date;

  i_debug("save date = %s", ctime(date_r));
  debug_print_mail(_mail, "rbox-mail::rbox_mail_get_save_date", NULL);
  debug_print_index_mail_data(data, "rbox-mail::rbox_mail_get_save_date", NULL);
  FUNC_END();
  return 0;
}

static int rbox_mail_get_physical_size(struct mail *_mail, uoff_t *size_r) {
  FUNC_START();
  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;

  uint64_t file_size = -1;
  time_t time = 0;

  i_debug("rbox_mail_get_physical_size(oid=%s, uid=%d):", rmail->mail_object->get_oid().c_str(), _mail->uid);

  rbox_get_index_record(_mail);
  if (index_mail_get_physical_size(_mail, size_r) == 0) {
    i_debug("rbox_mail_get_physical_size(oid=%s, uid=%d, size=%lu", rmail->mail_object->get_oid().c_str(), _mail->uid,
            *size_r);
    debug_print_mail(_mail, "rbox-mail::rbox_mail_get_physical_size (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");

    return 0;
  }

  if (rmail->mail_object->get_object_size() == -1) {
    if (((r_storage->s)->get_io_ctx()).stat(rmail->mail_object->get_oid(), &file_size, &time) < 0) {
      i_debug("no_object: rmail->mail_object->get_oid() %s, size %lu, uid=%d", rmail->mail_object->get_oid().c_str(),
              file_size, _mail->uid);

      FUNC_END_RET("ret == -1; rbox_read");
      return -1;
    }
    rmail->mail_object->set_object_size(file_size);
    i_debug("rmail->mail_object->get_oid() %s, size %lu, uid=%d", rmail->mail_object->get_oid().c_str(), file_size,
            _mail->uid);

  } else {
    file_size = rmail->mail_object->get_object_size();
    i_debug("rmail->mail_object->get_object_size() %s, size %lu, uid=%d", rmail->mail_object->get_oid().c_str(),
            file_size, _mail->uid);
  }

  *size_r = file_size;
  FUNC_END();
  return 0;
}

static int rbox_mail_get_stream(struct mail *_mail, bool get_body ATTR_UNUSED, struct message_size *hdr_size,
                                struct message_size *body_size, struct istream **stream_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct rbox_mail *rmail = (struct rbox_mail *)_mail;
  struct istream *input;

  struct rbox_storage *r_storage = (struct rbox_storage *)_mail->box->storage;
  unsigned int ret = 0;

  i_debug("rbox_mail_get_stream(oid=%s, uid=%d):", rmail->mail_object->get_oid().c_str(), _mail->uid);

  if (mail->data.stream == NULL) {
    uoff_t size_r = 0;

    if (rbox_mail_get_physical_size(_mail, &size_r) < 0) {
      FUNC_END_RET("ret == -1; get mail size");
      return -1;
    }

    if (size_r <= 0) {
      FUNC_END_RET("ret == -1; mail_size <= 0");
      return -1;
    }

    rmail->mail_buffer = p_new(default_pool, char, size_r + 1);
    if (rmail->mail_buffer == NULL) {
      FUNC_END_RET("ret == -1; out of memory");
      return -1;
    }

    memset(rmail->mail_buffer, '\0', sizeof(char) * size_r + 1);
    _mail->transaction->stats.open_lookup_count++;

    int offset = 0;
    librados::bufferlist mail_data_bl;

    std::string str_buf;
    do {
      mail_data_bl.clear();
      ret = ((r_storage->s)->get_io_ctx()).read(rmail->mail_object->get_oid(), mail_data_bl, size_r, offset);
      if (ret < 0) {
        FUNC_END_RET("ret == -1");
        return -1;
      }

      if (ret == 0) {
        break;
      }

      mail_data_bl.copy(0, ret, &rmail->mail_buffer[0]);

      offset += ret;
    } while (ret > 0);

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

void rbox_mail_free(struct mail *mail) {
  struct rbox_mail *rmail_ = (struct rbox_mail *)mail;
  debug_print_mail(mail, "rbox-mail::rbox_mail_free", NULL);

  if (rmail_->mail_buffer != NULL) {
    i_free(rmail_->mail_buffer);
  }
  if (rmail_->mail_object != 0) {
    delete rmail_->mail_object;
    rmail_->mail_object = NULL;
  }

  index_mail_free(mail);
}

void rbox_index_mail_set_seq(struct mail *_mail, uint32_t seq, bool saving) {
  struct rbox_mail *rmail_ = (struct rbox_mail *)_mail;

  if (rmail_->mail_buffer != NULL) {
    i_free(rmail_->mail_buffer);
  }
  if (rmail_->mail_object != 0) {
    delete rmail_->mail_object;
    rmail_->mail_object = NULL;
  }

  index_mail_set_seq(_mail, seq, saving);

  rmail_->mail_object = new RadosMailObject();
  // update stuff.
  rbox_get_index_record(_mail);

}

struct mail_vfuncs rbox_mail_vfuncs = {index_mail_close,
                                       rbox_mail_free,
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
                                       rbox_mail_get_physical_size, /* physical = virtual in our case */
                                       rbox_mail_get_physical_size,
                                       index_mail_get_first_header,
                                       index_mail_get_headers,
                                       index_mail_get_header_stream,
                                       rbox_mail_get_stream,
                                       index_mail_get_binary_stream,
                                       index_mail_get_special,
                                       index_mail_get_real_mail,
                                       index_mail_update_flags,
                                       index_mail_update_keywords,
                                       index_mail_update_modseq,
                                       index_mail_update_pvt_modseq,
                                       NULL,
                                       index_mail_expunge,
                                       index_mail_set_cache_corrupted,
                                       index_mail_opened,
                                       index_mail_set_cache_corrupted_reason};
