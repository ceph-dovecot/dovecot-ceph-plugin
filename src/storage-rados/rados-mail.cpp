/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include <map>
#include <string>
#include <iostream>
#include <vector>

extern "C" {

#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>

#include "lib.h"
#include "typeof-def.h"

#include "istream.h"
#include "index-mail.h"
#include "ioloop.h"
#include "str.h"

#include "rados-storage-local.h"
#include "ostream.h"
#include "debug-helper.h"
}

#include "rados-mail-object.h"
#include "rados-mail.h"
#include "rados-storage-struct.h"
#include "rados-storage.h"

using namespace librmb;  // NOLINT

int rados_get_index_record(struct mail *_mail) {
  FUNC_START();
  struct rados_mail *rmail = (struct rados_mail *)_mail;
  struct rados_mailbox *rbox = (struct rados_mailbox *)_mail->transaction->box;

  if (guid_128_is_empty(rmail->index_oid)) {
    const struct obox_mail_index_record *obox_rec;
    const void *rec_data;
    mail_index_lookup_ext(_mail->transaction->view, _mail->seq, rbox->ext_id, &rec_data, NULL);
    obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);

    if (obox_rec == nullptr) {
      /* lost for some reason, give up */
      FUNC_END_RET("ret == -1");
      return -1;
    }

    memcpy(rmail->index_guid, obox_rec->guid, sizeof(obox_rec->guid));
    memcpy(rmail->index_oid, obox_rec->oid, sizeof(obox_rec->oid));

    rmail->mail_object->set_oid(guid_128_to_string(rmail->index_oid));
  }

  FUNC_END();
  return 0;
}

struct mail *rados_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                              struct mailbox_header_lookup_ctx *wanted_headers) {
  FUNC_START();

  pool_t pool = pool_alloconly_create("mail", 2048);
  struct rados_mail *mail = p_new(pool, struct rados_mail, 1);
  mail->imail.mail.pool = pool;

  index_mail_init(&mail->imail, t, wanted_fields, wanted_headers);

  mail->mail_object = new RadosMailObject();

  FUNC_END();

  return &mail->imail.mail.mail;
}

static int rados_mail_get_metadata(struct mail *_mail) {
  FUNC_START();
  struct rados_storage *r_storage = (struct rados_storage *)_mail->box->storage;
  struct rados_mail *rmail = (struct rados_mail *)_mail;

  if (_mail->lookup_abort == MAIL_LOOKUP_ABORT_NOT_IN_CACHE) {
    mail_set_aborted(_mail);
    debug_print_mail(_mail, "rados-mail::rados_mail_stat (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }
  _mail->mail_metadata_accessed = TRUE;

  _mail->transaction->stats.stat_lookup_count++;

  rados_get_index_record(_mail);

  std::map<std::string, librados::bufferlist> attrset;
  int ret = ((r_storage->s)->get_io_ctx()).getxattrs(rmail->mail_object->get_oid(), attrset);
  if (ret < 0) {
    i_debug("rados_mail_get_metadata: getxattrs failed : oid: %s", rmail->mail_object->get_oid().c_str());
    FUNC_END_RET("ret == -1; rados getxattrs");
    return -1;
  }

  rmail->mail_object->set_state(attrset[RadosMailObject::X_ATTR_STATE].to_str());
  rmail->mail_object->set_version(attrset[RadosMailObject::X_ATTR_VERSION].to_str());

  guid_128_t guid = {};
  attrset[RadosMailObject::X_ATTR_GUID].copy(0, sizeof(guid), reinterpret_cast<char *>(guid));
  rmail->mail_object->set_guid(guid);

  rmail->mail_object->set_pop3_uidl(attrset[RadosMailObject::X_ATTR_POP3_UIDL].to_str());

  uint32_t pop3_order = 0;
  if (attrset[RadosMailObject::X_ATTR_POP3_ORDER].length() > 0) {
    attrset[RadosMailObject::X_ATTR_POP3_ORDER].copy(0, sizeof(pop3_order), reinterpret_cast<char *>(pop3_order));
  }
  rmail->mail_object->set_pop3_order(pop3_order);

  int length = 0;
  time_t received_date = 0;
  length = attrset[RadosMailObject::X_ATTR_RECEIVED_DATE].length();
  if (length > sizeof(received_date)) {
    FUNC_END_RET("ret == -1; value in X_ATTR_RECEIVED_DATE to long");
    return -1;
  }
  attrset[RadosMailObject::X_ATTR_RECEIVED_DATE].copy(0, length, reinterpret_cast<char *>(&received_date));
  rmail->mail_object->set_received_date(received_date);

  time_t send_date = 0;
  length = attrset[RadosMailObject::X_ATTR_SAVE_DATE].length();
  if (length > sizeof(send_date)) {
    FUNC_END_RET("ret == -1; value in X_ATTR_SAVE_DATE to long");
    return -1;
  }
  attrset[RadosMailObject::X_ATTR_SAVE_DATE].copy(0, length, reinterpret_cast<char *>(&send_date));
  rmail->mail_object->set_save_date(send_date);

  FUNC_END();
  return 0;
}

static int rados_mail_get_received_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct index_mail_data *data = &mail->data;
  struct rados_mail *rmail = (struct rados_mail *)_mail;

  if (index_mail_get_received_date(_mail, date_r) == 0) {
    i_debug("received date = %s", ctime(date_r));
    debug_print_mail(_mail, "rados-mail::rados_mail_get_received_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rados_mail_get_metadata(_mail) < 0) {
    debug_print_mail(_mail, "rados_mail_get_received_date: rados_mail_get_metadata failed (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  data->received_date = rmail->mail_object->get_received_date();
  *date_r = data->received_date;

  i_debug("received date = %s", ctime(date_r));
  debug_print_mail(_mail, "rados-mail::rados_mail_get_received_date", NULL);
  debug_print_index_mail_data(data, "rados-mail::rados_mail_get_received_date", NULL);
  FUNC_END();
  return 0;
}

static int rados_mail_get_save_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct index_mail_data *data = &mail->data;
  struct rados_mail *rmail = (struct rados_mail *)_mail;

  if (index_mail_get_save_date(_mail, date_r) == 0) {
    i_debug("save date = %s", ctime(date_r));
    debug_print_mail(_mail, "rados_mail_get_save_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rados_mail_get_metadata(_mail) < 0) {
    debug_print_mail(_mail, "rados_mail_get_save_date: rados_mail_get_metadata failed (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  data->save_date = rmail->mail_object->get_save_date();
  *date_r = data->save_date;

  i_debug("save date = %s", ctime(date_r));
  debug_print_mail(_mail, "rados-mail::rados_mail_get_save_date", NULL);
  debug_print_index_mail_data(data, "rados-mail::rados_mail_get_save_date", NULL);
  FUNC_END();
  return 0;
}

static int rados_mail_get_physical_size(struct mail *_mail, uoff_t *size_r) {
  FUNC_START();
  struct rados_storage *r_storage = (struct rados_storage *)_mail->box->storage;
  struct rados_mail *rmail = (struct rados_mail *)_mail;

  uint64_t file_size;
  time_t time;

  rados_get_index_record(_mail);
  if (index_mail_get_physical_size(_mail, size_r) == 0) {
    debug_print_mail(_mail, "rados-mail::rados_mail_get_physical_size (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }
  i_debug("rmail->mail_object->get_oid() %s", rmail->mail_object->get_oid().c_str());

  if (((r_storage->s)->get_io_ctx()).stat(rmail->mail_object->get_oid(), &file_size, &time) < 0) {
    FUNC_END_RET("ret == -1; rados_read");
    return -1;
  }

  *size_r = file_size;
  FUNC_END();
  return 0;
}
static int rados_mail_get_stream(struct mail *_mail, bool get_body ATTR_UNUSED, struct message_size *hdr_size,
                                 struct message_size *body_size, struct istream **stream_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct rados_mail *rmail = (struct rados_mail *)_mail;
  struct istream *input;

  struct rados_storage *r_storage = (struct rados_storage *)_mail->box->storage;
  int ret = 0;

  if (mail->data.stream == NULL) {
    uoff_t size_r = 0;

    if (rados_mail_get_physical_size(_mail, &size_r) < 0) {
      FUNC_END_RET("ret == -1; get mail size");
      return -1;
    }

    if (size_r <= 0) {
      i_debug("size is: %d", size_r);
      FUNC_END_RET("ret == -1; mail_size <= 0");
      return -1;
   }

   rmail->mail_buffer = p_new(default_pool, char, size_r);
   if (rmail->mail_buffer == NULL) {
     FUNC_END_RET("ret == -1; out of memory");
     return -1;
   }
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
     memcpy(&rmail->mail_buffer[offset], mail_data_bl.to_str().c_str(), ret * sizeof(char));
     offset += ret;

    } while (ret > 0);

    input = i_stream_create_from_data(rmail->mail_buffer, size_r);
    i_stream_set_name(input, RadosMailObject::DATA_BUFFER_NAME.c_str());
    index_mail_set_read_buffer_size(_mail, input);

    if (mail->mail.v.istream_opened != NULL) {
      if (mail->mail.v.istream_opened(_mail, &input) < 0) {
        i_stream_unref(&input);
        debug_print_mail(_mail, "rados-mail::rados_mail_get_stream (ret -1, 2)", NULL);
        FUNC_END_RET("ret == -1");
        return -1;
      }
    }
    mail->data.stream = input;
  }

  ret = index_mail_init_stream(mail, hdr_size, body_size, stream_r);
  debug_print_mail(_mail, "rados-mail::rados_mail_get_stream", NULL);
  FUNC_END();
  i_debug("ok i'm done ");
  return ret;
}
void rados_mail_free(struct mail *mail) {
  struct rados_mail *rmail_ = (struct rados_mail *)mail;

  free(rmail_->mail_buffer);
  if (rmail_->mail_object != 0) {
    delete rmail_->mail_object;
    rmail_->mail_object = NULL;
  }

  index_mail_free(mail);
}

void rados_mail_expunge(struct mail *mail) {
  struct rados_mail *rmail_ = (struct rados_mail *)mail;
  index_mail_expunge(mail);
}

struct mail_vfuncs rados_mail_vfuncs = {index_mail_close,
                                        rados_mail_free,
                                        index_mail_set_seq,
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
                                        rados_mail_get_received_date,
                                        rados_mail_get_save_date,
                                        rados_mail_get_physical_size, /* physical = virtual in our case */
                                        rados_mail_get_physical_size,
                                        index_mail_get_first_header,
                                        index_mail_get_headers,
                                        index_mail_get_header_stream,
                                        rados_mail_get_stream,
                                        index_mail_get_binary_stream,
                                        index_mail_get_special,
                                        index_mail_get_real_mail,
                                        index_mail_update_flags,
                                        index_mail_update_keywords,
                                        index_mail_update_modseq,
                                        index_mail_update_pvt_modseq,
                                        NULL,
                                        rados_mail_expunge,
                                        index_mail_set_cache_corrupted,
                                        index_mail_opened,
                                        index_mail_set_cache_corrupted_reason};
