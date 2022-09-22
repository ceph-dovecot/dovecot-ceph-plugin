// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
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
#include <map>
#include "rados-cluster.h"
namespace librmb {

class RadosClusterImpl : public RadosCluster {
 public:
  RadosClusterImpl();
  virtual ~RadosClusterImpl();

  int init() override;
  int init(const std::string &clustername, const std::string &rados_username) override;

  int connect();
  void deinit() override;

  int pool_create(const std::string &pool) override;
  int io_ctx_create(const std::string &pool, librados::IoCtx *io_ctx) override;
  int get_config_option(const char *option, std::string *value) override;
  int dictionary_create(const std::string &pool, const std::string &username, const std::string &oid,
                        RadosDictionary **dictionary);
  bool is_connected() override;
  librados::Rados &get_cluster() { return *cluster; }
  void set_config_option(const char *option, const char *value);

  std::vector<std::string> list_pgs_for_pool(std::string &pool_name) override;
  std::map<std::string, std::vector<std::string>> list_pgs_osd_for_pool(std::string &pool_name) override;

 private:
  int initialize();

 private:
  static librados::Rados *cluster;
  static int cluster_ref_count;
  static bool connected;
  std::map<const char *, const char *> client_options;

  static const char *CLIENT_MOUNT_TIMEOUT;
  static const char *RADOS_MON_OP_TIMEOUT;
  static const char *RADOS_OSD_OP_TIMEOUT;

  static const char *CLIENT_MOUNT_TIMEOUT_DEFAULT;
  static const char *RADOS_MON_OP_TIMEOUT_DEFAULT;
  static const char *RADOS_OSD_OP_TIMEOUT_DEFAULT;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_CLUSTER_IMPL_H_
