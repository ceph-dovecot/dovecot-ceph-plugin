/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <fcntl.h>

extern "C" {
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include "lib.h"
#include "typeof-def.h"

#include "istream.h"
#include "index-mail.h"
#include "ioloop.h"
#include "str.h"

#include "rados-storage-local.h"

#include "debug-helper.h"
}

#include "rados-mail.h"
#include "rados-storage-struct.h"
#include "rados-storage.h"

struct rados_mail {
  struct index_mail imail;

  guid_128_t mail_guid;
  guid_128_t mail_oid;
};

static int rados_get_guid(struct mail *_mail) {
  FUNC_START();
  struct rados_mail *mail = (struct rados_mail *)_mail;
  struct rados_mailbox *rbox = (struct rados_mailbox *)_mail->transaction->box;

  if (guid_128_is_empty(mail->mail_oid)) {
    const struct obox_mail_index_record *obox_rec;
    const void *rec_data;
    mail_index_lookup_ext(_mail->transaction->view, _mail->seq, rbox->ext_id, &rec_data, NULL);
    obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);

    if (obox_rec == nullptr) {
      /* lost for some reason, give up */
      FUNC_END_RET("ret == -1");
      return -1;
    }

    memcpy(mail->mail_guid, obox_rec->guid, sizeof(obox_rec->guid));
    memcpy(mail->mail_oid, obox_rec->oid, sizeof(obox_rec->oid));
  }

  i_debug("rados_get_guid: mail_guid=%s", guid_128_to_string(mail->mail_guid));
  i_debug("rados_get_guid: mail_oid=%s", guid_128_to_string(mail->mail_oid));

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

  FUNC_END();

  return &mail->imail.mail.mail;
}

static const char *rados_mail_get_path(struct mail *_mail) {
  FUNC_START();
  struct rados_mail *mail = (struct rados_mail *)_mail;

  const char *dir;

  rados_get_guid((struct mail *)mail);

  dir = mailbox_get_path(_mail->box);
  debug_print_mail(_mail, "rados-mail::rados_mail_get_path", NULL);
  const char *path = t_strdup_printf("%s/%s", dir, guid_128_to_string(mail->mail_oid));
  i_debug("path = %s", path);

  FUNC_END();
  return path;
}

static int rados_mail_stat(struct mail *mail, struct stat *st_r) {
  FUNC_START();
  const char *path;

  if (mail->lookup_abort == MAIL_LOOKUP_ABORT_NOT_IN_CACHE) {
    mail_set_aborted(mail);
    debug_print_mail(mail, "rados-mail::rados_mail_stat (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }
  mail->mail_metadata_accessed = TRUE;

  mail->transaction->stats.stat_lookup_count++;
  path = rados_mail_get_path(mail);
  if (stat(path, st_r) < 0) {
    if (errno == ENOENT) {
      mail_set_expunged(mail);
    } else {
      mail_storage_set_critical(mail->box->storage, "stat(%s) failed: %m", path);
    }
    debug_print_mail(mail, "rados-mail::rados_mail_stat (ret -1, 2)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }
  i_debug("stat filesize = %ld bytes", st_r->st_size);
  debug_print_mail(mail, "rados-mail::rados_mail_stat", NULL);
  FUNC_END();
  return 0;
}

static int rados_mail_get_received_date(struct mail *_mail, time_t *date_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct index_mail_data *data = &mail->data;
  struct stat st;

  if (index_mail_get_received_date(_mail, date_r) == 0) {
    i_debug("received date = %s", ctime(date_r));
    debug_print_mail(_mail, "rados-mail::rados_mail_get_received_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rados_mail_stat(_mail, &st) < 0) {
    debug_print_mail(_mail, "rados-mail::rados_mail_get_received_date (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  data->received_date = st.st_mtime;
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
  struct stat st;

  if (index_mail_get_save_date(_mail, date_r) == 0) {
    i_debug("save date = %s", ctime(date_r));
    debug_print_mail(_mail, "rados-mail::rados_mail_get_save_date (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  if (rados_mail_stat(_mail, &st) < 0) {
    debug_print_mail(_mail, "rados-mail::rados_mail_get_save_date (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  data->save_date = st.st_ctime;
  *date_r = data->save_date;
  i_debug("save date = %s", ctime(date_r));
  debug_print_mail(_mail, "rados-mail::rados_mail_get_save_date", NULL);
  debug_print_index_mail_data(data, "rados-mail::rados_mail_get_save_date", NULL);
  FUNC_END();
  return 0;
}

static int rados_mail_get_physical_size(struct mail *_mail, uoff_t *size_r) {
  FUNC_START();
  struct rados_storage *rados_storage = (struct rados_storage *)_mail->box->storage;
  struct rados_mail *r_mail = (struct rados_mail *)_mail;

  uint64_t file_size;
  time_t time;

  if (index_mail_get_physical_size(_mail, size_r) == 0) {
    i_debug("physical size = %lu", *size_r);
    debug_print_mail(_mail, "rados-mail::rados_mail_get_physical_size (ret 0, 1)", NULL);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  i_debug("JRSE_ oid: %s", guid_128_to_string(r_mail->mail_guid));
  if (((rados_storage->s)->get_io_ctx()).stat(guid_128_to_string(r_mail->mail_guid), &file_size, &time) < 0) {
    i_debug("read file stat from rados failed : oid: %s", guid_128_to_string(r_mail->mail_guid));
    FUNC_END_RET("ret == -1; rados_read");
    return -1;
  }

  *size_r = file_size;
  i_debug("rbox_mail_get_physical_size: size = %lu", *size_r);
  FUNC_END();
  return 0;
}
static int rados_mail_get_stream(struct mail *_mail, bool get_body ATTR_UNUSED, struct message_size *hdr_size,
                                 struct message_size *body_size, struct istream **stream_r) {
  FUNC_START();
  struct index_mail *mail = (struct index_mail *)_mail;
  struct rados_mail *r_mail = (struct rados_mail *)_mail;
  struct istream *input;

  struct rados_storage *storage = (struct rados_storage *)_mail->box->storage;
  int ret = 0;

  if (mail->data.stream == NULL) {
    i_debug("mail_guid before : %s", guid_128_to_string(r_mail->mail_oid));
    // read guid
    rados_get_guid(_mail);
    i_debug("mail_guid after : %s", guid_128_to_string(r_mail->mail_oid));

    uoff_t size_r = 0;
    if (rados_mail_get_physical_size(_mail, &size_r) < 0 || size_r == 0) {
      i_debug("error fetching mails physical size_r is: %ld ", size_r);
      return -1;
    }
    i_debug("found mail with %d bytes", size_r);
    _mail->transaction->stats.open_lookup_count++;

    librados::bufferlist bl;
    do {
      ret = ((storage->s)->get_io_ctx()).read(guid_128_to_string(r_mail->mail_oid), bl, size_r, ret);
      if (ret <= 0) {
        return -1;
      }
    } while (ret < size_r);

    // create buffer on heap for istream
    // TODO(jrse) check if this creates a memory leak?
    char *copy = i_malloc(bl.length() + 1);
    strcpy(copy, bl.to_str().c_str());
    i_debug("mail is: %s", copy);
    input = i_stream_create_from_data(copy, size_r);
    i_stream_set_name(input, "RADOS");
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
  i_debug("stream offset = %lu", (*stream_r)->v_offset);
  debug_print_mail(_mail, "rados-mail::rados_mail_get_stream", NULL);
  FUNC_END();
  return ret;
}

struct mail_vfuncs rados_mail_vfuncs = {index_mail_close,
                                        index_mail_free,
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
                                        index_mail_expunge,
                                        index_mail_set_cache_corrupted,
                                        index_mail_opened,
                                        index_mail_set_cache_corrupted_reason};
