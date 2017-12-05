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

#ifndef SRC_LIBRMB_RADOS_CLUSTER_IMPL_H_
#define SRC_LIBRMB_RADOS_CLUSTER_IMPL_H_

#include <list>
#include <string>

#include <rados/librados.hpp>

#include "rados-cluster.h"
namespace librmb {

class RadosClusterImpl : public RadosCluster {
 public:
  RadosClusterImpl();
  virtual ~RadosClusterImpl();

  int init();
  int init(const std::string &clustername, const std::string &rados_username);

  int connect();
  void deinit();

  int pool_create(const std::string &pool);
  int io_ctx_create(const std::string &pool, librados::IoCtx *io_ctx);
  int get_config_option(const char *option, std::string *value);
  int dictionary_create(const std::string &pool, const std::string &username, const std::string &oid,
                        RadosDictionary **dictionary);

  bool is_connected();

  librados::Rados &get_cluster() { return cluster; }

 private:
  int initialize();

 private:
  librados::Rados cluster;
  int cluster_ref_count;
  bool connected;

  static const char *CLIENT_MOUNT_TIMEOUT;
  static const char *RADOS_MON_OP_TIMEOUT;
  static const char *RADOS_OSD_OP_TIMEOUT;

  static const char *CLIENT_MOUNT_TIMEOUT_DEFAULT;
  static const char *RADOS_MON_OP_TIMEOUT_DEFAULT;
  static const char *RADOS_OSD_OP_TIMEOUT_DEFAULT;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_CLUSTER_IMPL_H_
