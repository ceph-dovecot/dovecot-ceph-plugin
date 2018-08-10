// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_STORAGE_RBOX_RBOX_MAIL_H_
#define SRC_STORAGE_RBOX_RBOX_MAIL_H_

#include "index-mail.h"
#include <rados/librados.hpp>
#include "../librmb/rados-mail.h"

struct rbox_mail {
  struct index_mail imail;

  guid_128_t index_guid;
  guid_128_t index_oid;

  librmb::RadosMail *rados_mail;
  uint32_t last_seq;  // TODO(jrse): init with -1
};
extern void rbox_mail_set_expunged(struct rbox_mail *mail);
extern int rbox_get_index_record(struct mail *_mail);
extern struct mail *rbox_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
                                    struct mailbox_header_lookup_ctx *wanted_headers);
extern int rbox_mail_get_virtual_size(struct mail *_mail, uoff_t *size_r);

#endif  // SRC_STORAGE_RBOX_RBOX_MAIL_H_
