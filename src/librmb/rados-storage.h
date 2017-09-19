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

#include <map>
#include <string>
#include <cstdint>

#include <rados/librados.hpp>
#include "interfaces/rados-storage-interface.h"
#include "rados-mail-object.h"

namespace librmb {

class RadosStorageImpl : public RadosStorage {
 public:
  explicit RadosStorageImpl(RadosCluster *cluster);
  virtual ~RadosStorageImpl();

  librados::IoCtx &get_io_ctx();
  int stat_mail(const std::string &oid, uint64_t *psize, time_t *pmtime);
  void set_namespace(const std::string &_nspace);
  std::string get_namespace() { return nspace; }
  int get_max_write_size() { return max_write_size; }
  int get_max_write_size_bytes() { return max_write_size * 1024 * 1024; }

  int split_buffer_and_exec_op(const char *buffer, size_t buffer_length, RadosMailObject *current_object,
                               librados::ObjectWriteOperation *write_op_xattr, uint64_t max_write);

  int read_mail(const std::string &oid, uint64_t *size_r, char *mail_buffer);
  int load_metadata(RadosMailObject *mail);
  int set_metadata(const std::string &oid, const RadosXAttr &xattr);

  int delete_mail(RadosMailObject *mail);
  int delete_mail(std::string oid);

  int aio_operate(librados::IoCtx *io_ctx_, const std::string &oid, librados::AioCompletion *c,
                  librados::ObjectWriteOperation *op);
  librados::NObjectIterator find_mails(RadosXAttr *attr);
  int open_connection(const std::string &poolname, const std::string &ns);

  bool wait_for_write_operations_complete(
      std::map<librados::AioCompletion *, librados::ObjectWriteOperation *> *completion_op_map);

 private:
  RadosCluster *cluster;
  int max_write_size;
  std::string nspace;
  librados::IoCtx io_ctx;

  static const char *CFG_OSD_MAX_WRITE_SIZE;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_STORAGE_H_
