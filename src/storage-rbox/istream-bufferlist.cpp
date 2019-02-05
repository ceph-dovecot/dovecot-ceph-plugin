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

extern "C" {
#include "lib.h"
#include "istream-private.h"
}

#include "istream-bufferlist.h"
#include <rados/librados.hpp>

static ssize_t i_stream_data_read(struct istream_private *stream) {
  stream->istream.eof = TRUE;  // all in!
  return -1;
}

static void i_stream_data_seek(struct istream_private *stream, uoff_t v_offset, bool mark ATTR_UNUSED) {
  stream->skip = v_offset;
  stream->istream.v_offset = v_offset;
}

static void rbox_istream_destroy(struct iostream_private *stream) {
  // nothing to do. but required, so that default destroy is not evoked!
  // buffer is member of RboxMailObjec, which destroys the bufferlist
}
struct istream *i_stream_create_from_bufferlist(librados::bufferlist *data, const size_t &size) {
  struct istream_private *stream;

  stream = i_new(struct istream_private, 1);
  // use unsigned char* for binary data!
  stream->buffer = reinterpret_cast<const unsigned char *>(data->c_str());
  stream->pos = size;
  stream->max_buffer_size = (size_t)-1;

  stream->read = i_stream_data_read;
  stream->seek = i_stream_data_seek;  // use default

  stream->istream.readable_fd = FALSE;
  stream->istream.blocking = TRUE;
  stream->istream.seekable = TRUE;
  stream->iostream.destroy = rbox_istream_destroy;

#if DOVECOT_PREREQ(2, 3)
  i_stream_create(stream, NULL, -1, ISTREAM_CREATE_FLAG_NOOP_SNAPSHOT);
#else
  i_stream_create(stream, NULL, -1);
#endif
  stream->statbuf.st_size = size - 1;
  i_stream_set_name(&stream->istream, "(buffer)");
  return &stream->istream;
}
