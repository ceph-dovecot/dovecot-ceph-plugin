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
#include <string>
#include <list>

extern "C" {
#include "lib.h"
#include "ostream-private.h"
}
#include "ostream-bufferlist.h"

struct bufferlist_ostream {
  struct ostream_private ostream;
  librados::bufferlist *buf;
  bool seeked;
  librmb::RadosStorage *rados_storage;
  librmb::RadosMail *rados_mail;
  bool execute_write_ops;
};

static int o_stream_buffer_seek(struct ostream_private *stream, uoff_t offset) {
  struct bufferlist_ostream *bstream = (struct bufferlist_ostream *)stream;
  bstream->seeked = TRUE;
  stream->ostream.offset = offset;
  return 1;
}

int o_stream_buffer_write_at(struct ostream_private *stream, const void *data, size_t size, uoff_t offset) {
  i_assert(stream != NULL);
  i_error("unused (o_stream_buffer_write_at) !");
  return -1;
}

static void rbox_ostream_destroy(struct iostream_private *stream) {
  // nothing to do. but required, so that default destroy is not evoked!
  // buffer is member of RboxMailObjec, which destroys the bufferlist

  // wait for write operation
  struct bufferlist_ostream *bstream = (struct bufferlist_ostream *)stream;
  i_assert(bstream->buf != nullptr);

  // do not free the outbut stream! cause, it is needed until all write operations are finished!
  // delete bstream->buf;
}

static ssize_t o_stream_buffer_sendv(struct ostream_private *stream, const struct const_iovec *iov,
                                     unsigned int iov_count) {
  struct bufferlist_ostream *bstream = (struct bufferlist_ostream *)stream;
  ssize_t ret = 0;
  unsigned int i;
  uint64_t val = stream->ostream.offset;
  if (bstream->execute_write_ops) {
    bstream->buf->clear();
  }
  for (i = 0; i < iov_count; i++) {
    // use unsigned char* for binary data!
    bstream->buf->append(reinterpret_cast<const unsigned char *>(iov[i].iov_base), iov[i].iov_len);
    stream->ostream.offset += iov[i].iov_len;
    ret += iov[i].iov_len;
  }

  if (bstream->execute_write_ops) {
    librados::ObjectWriteOperation write_op;
    write_op.write(val, *bstream->buf);

    bstream->rados_storage->aio_operate(&bstream->rados_storage->get_io_ctx(), *bstream->rados_mail->get_oid(),
                                        bstream->rados_mail->get_completion(), &write_op);
  }
  return ret;
}

struct ostream *o_stream_create_bufferlist(librmb::RadosMail *rados_mail, librmb::RadosStorage *rados_storage,
                                           bool execute_write_ops) {
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
  bstream->ostream.iostream.destroy = rbox_ostream_destroy;
  //  bstream->buf = execute_write_ops ? new librados::bufferlist() : rados_mail->get_mail_buffer();
  bstream->buf = rados_mail->get_mail_buffer();
  bstream->rados_storage = rados_storage;
  bstream->rados_mail = rados_mail;
  bstream->execute_write_ops = execute_write_ops;
  if (execute_write_ops) {
    rados_mail->set_completion(librados::Rados::aio_create_completion());
    rados_mail->set_active_op(1);
  }
  output = o_stream_create(&bstream->ostream, NULL, -1);
  o_stream_set_name(output, "(buffer)");
  return output;
}
