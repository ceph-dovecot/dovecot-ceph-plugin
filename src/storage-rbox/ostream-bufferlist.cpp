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

extern "C" {
#include "lib.h"
#include "ostream-private.h"
}
#include "ostream-bufferlist.h"


struct bufferlist_ostream {
  struct ostream_private ostream;
  librados::bufferlist *buf;
  bool seeked;
};

static int o_stream_buffer_seek(struct ostream_private *stream, uoff_t offset) {
  struct bufferlist_ostream *bstream = (struct bufferlist_ostream *)stream;
  bstream->seeked = TRUE;
  stream->ostream.offset = offset;
  return 1;
}

static int o_stream_buffer_write_at(struct ostream_private *stream, const void *data, size_t size, uoff_t offset) {
  struct bufferlist_ostream *bstream = (struct bufferlist_ostream *)stream;
  // ceph::bufferptr bp = ceph::buffer::create_static(size, reinterpret_cast<const char *>(data));
  // bstream->buf->append(bp, offset, size);
  bstream->buf->append(reinterpret_cast<const char *>(data), size);
  return 0;
}

static ssize_t o_stream_buffer_sendv(struct ostream_private *stream, const struct const_iovec *iov,
                                     unsigned int iov_count) {
  struct bufferlist_ostream *bstream = (struct bufferlist_ostream *)stream;
  ssize_t ret = 0;
  unsigned int i;

  for (i = 0; i < iov_count; i++) {
    bstream->buf->append(reinterpret_cast<const char *>(iov[i].iov_base), iov[i].iov_len);
    stream->ostream.offset += iov[i].iov_len;
    ret += iov[i].iov_len;
  }
  return ret;
}

struct ostream *o_stream_create_bufferlist(librados::bufferlist *buf) {
  struct bufferlist_ostream *bstream;
  struct ostream *output;

  bstream = i_new(struct bufferlist_ostream, 1);
  /* we don't set buffer as blocking, because if max_buffer_size is
     changed it can get truncated. this is used in various places in
     unit tests. */
  bstream->ostream.max_buffer_size = (size_t)-1;
  bstream->ostream.seek = o_stream_buffer_seek;
  bstream->ostream.sendv = o_stream_buffer_sendv;
  bstream->ostream.write_at = o_stream_buffer_write_at;

  bstream->buf = buf;
  output = o_stream_create(&bstream->ostream, NULL, -1);
  o_stream_set_name(output, "(buffer)");
  return output;
}
