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

namespace librmb {

class RadosDictionary;
class RadosStorage;

class RadosCluster {
 public:
  RadosCluster();
  virtual ~RadosCluster();

  int init(std::string *error_r);
  void deinit();

  int pool_create(const std::string &pool);
  int dictionary_create(const std::string &pool, const std::string &username, const std::string &oid,
                        RadosDictionary **dictionary);
  int storage_create(const std::string &pool, RadosStorage **storage);

  int open_connection(RadosStorage **storage, const std::string &poolname, const std::string &ns);

 private:
  static librados::Rados cluster;
  static int cluster_ref_count;

 public:
  static const char *CFG_OSD_MAX_WRITE_SIZE;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_CLUSTER_H_
