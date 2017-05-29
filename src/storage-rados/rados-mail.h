/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RADOS_RADOS_MAIL_H_
#define SRC_STORAGE_RADOS_RADOS_MAIL_H_

#include "index-mail.h"

extern struct mail *rados_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                                     struct mailbox_header_lookup_ctx *wanted_headers);

#endif  // SRC_STORAGE_RADOS_RADOS_MAIL_H_
