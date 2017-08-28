/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_RBOX_MAIL_H_
#define SRC_STORAGE_RBOX_RBOX_MAIL_H_

#include "index-mail.h"
#include "rados-mail-object.h"

struct rbox_mail {
  struct index_mail imail;

  guid_128_t index_guid;
  guid_128_t index_oid;

  librmb::RadosMailObject *mail_object;
  char *mail_buffer;
  uint32_t last_seq;  // TODO(jrse): init with -1
};

extern int rbox_get_index_record(struct mail *_mail);
extern struct mail *rbox_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                                    struct mailbox_header_lookup_ctx *wanted_headers);
extern int rbox_mail_get_virtual_size(struct mail *_mail, uoff_t *size_r);

#endif  // SRC_STORAGE_RBOX_RBOX_MAIL_H_
