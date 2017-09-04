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

#include <string>

#include <rados/librados.hpp>
#include "rados-storage.h"

using std::string;

using librmb::RadosStorage;

#define DICT_USERNAME_SEPARATOR '/'

RadosStorage::RadosStorage(librados::IoCtx *_ctx, const int _max_write_size)
    : io_ctx(*_ctx), max_write_size(_max_write_size) {}

RadosStorage::~RadosStorage() { get_io_ctx().close(); }

int RadosStorage::split_buffer_and_exec_op(const char *buffer, size_t buffer_length, RadosMailObject *current_object,
                                           librados::ObjectWriteOperation *write_op_xattr, uint64_t max_write) {
  size_t write_buffer_size = buffer_length;
  int ret_val = 0;
  assert(max_write > 0);

  int rest = write_buffer_size % max_write;
  int div = write_buffer_size / max_write + (rest > 0 ? 1 : 0);
  for (int i = 0; i < div; i++) {
    int offset = i * max_write;

    librados::ObjectWriteOperation *op = i == 0 ? write_op_xattr : new librados::ObjectWriteOperation();

    uint64_t length = max_write;
    if (buffer_length < ((i + 1) * length)) {
      length = rest;
    }
    const char *buf = buffer + offset;
    librados::bufferlist tmp_buffer;
    tmp_buffer.append(buf, length);
    op->write(offset, tmp_buffer);

    librados::AioCompletion *completion = librados::Rados::aio_create_completion();
    completion->set_complete_callback(current_object, nullptr);

    (*current_object->get_completion_op_map())[completion] = op;

    ret_val = io_ctx.aio_operate(current_object->get_oid(), completion, op);
    if (ret_val < 0) {
      break;
    }
  }

  return ret_val;
}

int RadosStorage::read_mail(const std::string &oid, uint64_t *size_r, char *mail_buffer) {
  int offset = 0;
  librados::bufferlist mail_data_bl;

  std::string str_buf;
  int ret = 0;
  do {
    mail_data_bl.clear();
    ret = io_ctx.read(oid, mail_data_bl, *size_r, offset);
    if (ret < 0) {
      return ret;
    }
    if (ret == 0) {
      break;
    }
    mail_data_bl.copy(0, (unsigned)ret, mail_buffer);
    offset += ret;
  } while (ret > 0);
  return ret;
}

int RadosStorage::load_xattr(RadosMailObject *mail) {
  int ret = -1;

  if (mail != nullptr) {
    if (mail->get_xattr()->size() == 0) {
      ret = io_ctx.getxattrs(mail->get_oid(), *mail->get_xattr());
    } else {
      ret = 0;
    }
  }
  return ret;
}

int RadosStorage::delete_mail(RadosMailObject *mail) {
  int ret = -1;
  if (mail != nullptr) {
    ret = io_ctx.remove(mail->get_oid());
  }
  return ret;
}
