/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RADOS_RADOS_MAIL_H_
#define SRC_STORAGE_RADOS_RADOS_MAIL_H_

#include "index-mail.h"

#include "rados-mail-object.h"

struct rados_mail {
  struct index_mail imail;

  guid_128_t index_guid;
  guid_128_t index_oid;

  librmb::RadosMailObject *mail_object;
  char *mail_buffer;
};

extern int rados_get_index_record(struct mail *_mail);
extern struct mail *rados_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                                     struct mailbox_header_lookup_ctx *wanted_headers);

#endif  // SRC_STORAGE_RADOS_RADOS_MAIL_H_
