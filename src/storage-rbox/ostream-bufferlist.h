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

#ifndef SRC_STORAGE_RBOX_OSTREAM_BUFFERLIST_H_
#define SRC_STORAGE_RBOX_OSTREAM_BUFFERLIST_H_
#include <rados/librados.hpp>

struct ostream *o_stream_create_bufferlist(librados::bufferlist *buf);

#endif /* SRC_STORAGE_RBOX_OSTREAM_BUFFERLIST_H_ */
