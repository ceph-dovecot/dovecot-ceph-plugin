// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_LIBRMB_RADOS_STORAGE_H_
#define SRC_LIBRMB_RADOS_STORAGE_H_

#include <stddef.h>

#include <list>
#include <string>
#include <cstdint>

#include <rados/librados.hpp>
#include "rados-mail-object.h"

namespace librmb {

class RadosStorage {
 public:
  RadosStorage(librados::IoCtx *ctx, const int max_write_size);
  virtual ~RadosStorage();

  librados::IoCtx &get_io_ctx() { return io_ctx; }

  int get_max_write_size() { return max_write_size; }
  int get_max_write_size_bytes() { return max_write_size * 1024 * 1024; }

  int split_buffer_and_exec_op(const char *buffer, size_t buffer_length, RadosMailObject *current_object,
                               librados::ObjectWriteOperation *write_op_xattr, uint64_t max_write);

  int read_mail(const std::string &oid, uint64_t *size_r, char *mail_buffer);
  int load_xattr(RadosMailObject *mail);
  int delete_mail(RadosMailObject *mail);

 private:
  librados::IoCtx io_ctx;
  int max_write_size;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_STORAGE_H_
