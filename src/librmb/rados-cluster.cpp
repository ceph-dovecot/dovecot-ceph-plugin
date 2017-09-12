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

#include <list>
#include <string>
#include <utility>

#include "rados-cluster.h"
#include "rados-dictionary.h"
#include "rados-storage.h"

using std::list;
using std::pair;
using std::string;

using librmb::RadosClusterImpl;

librados::Rados RadosClusterImpl::cluster;
int RadosClusterImpl::cluster_ref_count = 0;
bool RadosClusterImpl::connected = false;

RadosClusterImpl::RadosClusterImpl() {}

RadosClusterImpl::~RadosClusterImpl() {}

int RadosClusterImpl::init() {
  int ret = 0;
  if (cluster_ref_count == 0) {
    ret = cluster.init(nullptr);

    if (ret == 0) {
      ret = cluster.conf_parse_env(nullptr);
    }

    if (ret == 0) {
      ret = cluster.conf_read_file(nullptr);
    }

    if (ret == 0)
      cluster_ref_count++;
  }
  return ret;
}

int RadosClusterImpl::connect() {
  int ret = 0;
  if (cluster_ref_count > 0 && !connected) {
    ret = cluster.connect();
    connected = ret == 0;
  }
  return ret;
}

void RadosClusterImpl::deinit() {
  if (cluster_ref_count > 0) {
    if (--cluster_ref_count == 0) {
      if (connected) {
        cluster.shutdown();
        connected = false;
      }
    }
  }
}

int RadosClusterImpl::pool_create(const string &pool) {
  // pool exists? else create

  int ret = connect();
  if (ret == 0) {
    list<pair<int64_t, string>> pool_list;
    ret = cluster.pool_list2(pool_list);

    if (ret == 0) {
      bool pool_found = false;

      for (list<pair<int64_t, string>>::iterator it = pool_list.begin(); it != pool_list.end(); ++it) {
        if ((*it).second.compare(pool) == 0) {
          pool_found = true;
          break;
        }
      }

      if (pool_found != true) {
        ret = cluster.pool_create(pool.c_str());
        pool_found = ret == 0;
      }
    }
  }

  return ret;
}

int RadosClusterImpl::io_ctx_create(const string &pool, librados::IoCtx *io_ctx) {
  int ret = 0;

  assert(io_ctx != nullptr);

  if (cluster_ref_count == 0) {
    ret = -ENOENT;
  }

  if (ret == 0) {
    ret = connect();

    if (ret == 0) {
      // pool exists? else create
      ret = pool_create(pool);
    }

    if (ret == 0) {
      ret = cluster.ioctx_create(pool.c_str(), *io_ctx);
    }
  }

  return ret;
}

int RadosClusterImpl::get_config_option(const char *option, string *value) { return cluster.conf_get(option, *value); }
