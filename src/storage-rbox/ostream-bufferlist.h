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

#ifndef SRC_STORAGE_RBOX_OSTREAM_BUFFERLIST_H_
#define SRC_STORAGE_RBOX_OSTREAM_BUFFERLIST_H_
extern "C" {
#include "lib.h"
#include "ostream-private.h"
}
#include <rados/librados.hpp>
#include "rados-storage.h"
#include "rados-mail.h"

struct ostream *o_stream_create_bufferlist(librmb::RadosMail *rados_mail, librmb::RadosStorage *rados_storage,
                                           bool execute_write_ops);
int o_stream_buffer_write_at(struct ostream_private *stream, const void *data, size_t size, uoff_t offset);
#endif /* SRC_STORAGE_RBOX_OSTREAM_BUFFERLIST_H_ */
