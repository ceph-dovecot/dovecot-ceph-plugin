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

#ifndef SRC_LIBRMB_INTERFACES_RADOS_CLUSTER_INTERFACE_H_
#define SRC_LIBRMB_INTERFACES_RADOS_CLUSTER_INTERFACE_H_

#include <string>

#include <rados/librados.hpp>

namespace librmb {
class RadosDictionary;

class RadosCluster {
 public:
  virtual ~RadosCluster() {}
  virtual int init(std::string *error_r) = 0;
  virtual void deinit() = 0;

  virtual int pool_create(const std::string &pool) = 0;

  virtual int io_ctx_create(const std::string &pool) = 0;
  virtual int get_config_option(const char *option, std::string *value) = 0;

  virtual librados::IoCtx &get_io_ctx() = 0;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_INTERFACES_RADOS_CLUSTER_INTERFACE_H_
