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

#ifndef SRC_LIBRMB_RADOS_CLUSTER_H_
#define SRC_LIBRMB_RADOS_CLUSTER_H_

#include <string>

#include <rados/librados.hpp>
#include "interfaces/rados-cluster-interface.h"

namespace librmb {

class RadosClusterImpl : public RadosCluster {
 public:
  RadosClusterImpl();
  virtual ~RadosClusterImpl();

  int init(std::string *error_r);
  void deinit();

  int pool_create(const std::string &pool);
  int io_ctx_create(const std::string &pool);
  int get_config_option(const char *option, std::string *value);
  int dictionary_create(const std::string &pool, const std::string &username, const std::string &oid,
                        RadosDictionary **dictionary);

  librados::IoCtx &get_io_ctx() { return this->io_ctx; }

 private:
  static librados::Rados cluster;
  static int cluster_ref_count;
  librados::IoCtx io_ctx;

};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_CLUSTER_H_
