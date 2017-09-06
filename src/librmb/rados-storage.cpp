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
#include "encoding.h"

using std::string;

using librmb::RadosStorageImpl;

#define DICT_USERNAME_SEPARATOR '/'
const char *RadosStorageImpl::CFG_OSD_MAX_WRITE_SIZE = "osd_max_write_size";

RadosStorageImpl::RadosStorageImpl(RadosCluster *_cluster) {
  this->cluster = _cluster;
  this->max_write_size = 0;
}

RadosStorageImpl::~RadosStorageImpl() { cluster->get_io_ctx().close(); }

int RadosStorageImpl::split_buffer_and_exec_op(const char *buffer, size_t buffer_length,
                                               RadosMailObject *current_object,
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

    ret_val = cluster->get_io_ctx().aio_operate(current_object->get_oid(), completion, op);
    if (ret_val < 0) {
      break;
    }
  }

  return ret_val;
}

int RadosStorageImpl::read_mail(const std::string &oid, uint64_t *size_r, char *mail_buffer) {
  int offset = 0;
  librados::bufferlist mail_data_bl;

  std::string str_buf;
  int ret = 0;
  do {
    mail_data_bl.clear();
    ret = cluster->get_io_ctx().read(oid, mail_data_bl, *size_r, offset);
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

int RadosStorageImpl::load_xattr(RadosMailObject *mail) {
  int ret = -1;

  if (mail != nullptr) {
    if (mail->get_xattr()->size() == 0) {
      ret = cluster->get_io_ctx().getxattrs(mail->get_oid(), *mail->get_xattr());
    } else {
      ret = 0;
    }
  }
  return ret;
}

int RadosStorageImpl::set_xattr(const std::string &oid, RadosXAttr &xattr) {
  return cluster->get_io_ctx().setxattr(oid, xattr.key.c_str(), xattr.bl);
}

int RadosStorageImpl::delete_mail(RadosMailObject *mail) {
  int ret = -1;
  if (mail != nullptr) {
    ret = delete_mail(mail->get_oid());
  }
  return ret;
}
int RadosStorageImpl::delete_mail(std::string oid) {
  int ret = -1;
  if (!oid.empty()) {
    ret = cluster->get_io_ctx().remove(oid);
  }
  return ret;
}

int RadosStorageImpl::aio_operate(librados::IoCtx *io_ctx_, const std::string &oid, librados::AioCompletion *c,
                                  librados::ObjectWriteOperation *op) {
  if (io_ctx_ != nullptr) {
    return io_ctx_->aio_operate(oid, c, op);
  } else {
    return cluster->get_io_ctx().aio_operate(oid, c, op);
  }
}

int RadosStorageImpl::stat_object(const std::string &oid, uint64_t *psize, time_t *pmtime) {
  return cluster->get_io_ctx().stat(oid, psize, pmtime);
}
void RadosStorageImpl::set_namespace(const std::string &nspace) { cluster->get_io_ctx().set_namespace(nspace); }

librados::NObjectIterator RadosStorageImpl::find_objects(RadosXAttr *attr) {
  if (attr != nullptr) {
    std::string filter_name = PLAIN_FILTER_NAME;
    ceph::bufferlist filter_bl;

    encode(filter_name, filter_bl);
    encode("_" + attr->key, filter_bl);
    encode(attr->bl.to_str(), filter_bl);

    return cluster->get_io_ctx().nobjects_begin(filter_bl);
  } else {
    return cluster->get_io_ctx().nobjects_begin();
  }
}

int RadosStorageImpl::open_connection(const std::string &poolname, const std::string &ns) {
  std::string error_msg;
  if (cluster->init(&error_msg) < 0) {
    return -1;
  }
  // pool exists? else create
  int err = cluster->io_ctx_create(poolname);
  if (err < 0) {
    return err;
  }
  std::string max_write_size_str;
  err = cluster->get_config_option(RadosStorageImpl::CFG_OSD_MAX_WRITE_SIZE, &max_write_size_str);
  if (err < 0) {
    return err;
  }
  max_write_size = std::stoi(max_write_size_str);
  return 0;
}
