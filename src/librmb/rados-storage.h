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

#ifndef SRC_LIBRMB_INTERFACES_RADOS_STORAGE_INTERFACE_H_
#define SRC_LIBRMB_INTERFACES_RADOS_STORAGE_INTERFACE_H_

#include <string>
#include <map>

#include "rados-mail-object.h"
#include <rados/librados.hpp>
#include "rados-cluster.h"

namespace librmb {

class RadosStorage {
 public:
  virtual ~RadosStorage() {}

  virtual librados::IoCtx &get_io_ctx() = 0;
  virtual int stat_mail(const std::string &oid, uint64_t *psize, time_t *pmtime) = 0;
  virtual void set_namespace(const std::string &_nspace) = 0;
  virtual std::string get_namespace() = 0;
  virtual int get_max_write_size() = 0;
  virtual int get_max_write_size_bytes() = 0;

  virtual int split_buffer_and_exec_op(const char *buffer, size_t buffer_length, RadosMailObject *current_object,
                                       librados::ObjectWriteOperation *write_op_xattr, uint64_t max_write) = 0;

  virtual int load_metadata(RadosMailObject *mail) = 0;
  virtual int set_metadata(const std::string &oid, const RadosMetadata &xattr) = 0;

  virtual int delete_mail(RadosMailObject *mail) = 0;
  virtual int delete_mail(std::string oid) = 0;

  virtual int aio_operate(librados::IoCtx *io_ctx_, const std::string &oid, librados::AioCompletion *c,
                          librados::ObjectWriteOperation *op) = 0;
  virtual librados::NObjectIterator find_mails(const RadosMetadata *attr) = 0;
  virtual int open_connection(const std::string &poolname, const std::string &ns) = 0;
  virtual bool wait_for_write_operations_complete(
      std::map<librados::AioCompletion*, librados::ObjectWriteOperation*>* completion_op_map) = 0;
  virtual int read_mail(librados::bufferlist *buffer, const std::string &oid) = 0;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_INTERFACES_RADOS_STORAGE_INTERFACE_H_
