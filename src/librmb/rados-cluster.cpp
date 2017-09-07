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
RadosClusterImpl::RadosClusterImpl() {}

RadosClusterImpl::~RadosClusterImpl() {}

int RadosClusterImpl::init(string *error_r) {
  if (cluster_ref_count == 0) {
    int ret = 0;
    ret = cluster.init(nullptr);
    if (ret < 0) {
      *error_r = "Couldn't create the cluster handle! " + string(strerror(-ret));
      return ret;
    }

    ret = cluster.conf_parse_env(nullptr);
    if (ret < 0) {
      *error_r = "Cannot parse config environment! " + string(strerror(-ret));
      return ret;
    }

    ret = cluster.conf_read_file(nullptr);
    if (ret < 0) {
      *error_r = "Cannot read config file! " + string(strerror(-ret));
      return ret;
    }

    ret = cluster.connect();
    if (ret < 0) {
      *error_r = "Cannot connect to cluster! " + string(strerror(-ret));
      return ret;
    } else {
      cluster_ref_count++;
    }
  }

  return 0;
}

void RadosClusterImpl::deinit() {
  if (cluster_ref_count > 0) {
    cluster_ref_count--;
    if (cluster_ref_count == 0) {
      io_ctx.close();
      cluster.shutdown();
    }
  }
}

int RadosClusterImpl::pool_create(const string &pool) {
  // pool exists? else create
  list<pair<int64_t, string>> pool_list;
  int err = cluster.pool_list2(pool_list);
  if (err < 0) {
    // *error_r = t_strdup_printf("Cannot list RADOS pools: %s", strerror(-err));
    return err;
  }

  bool pool_found = false;
  for (list<pair<int64_t, string>>::iterator it = pool_list.begin(); it != pool_list.end(); ++it) {
    if ((*it).second.compare(pool) == 0) {
      pool_found = true;
      break;
    }
  }

  if (pool_found != true) {
    err = cluster.pool_create(pool.c_str());
    if (err < 0) {
      // *error_r = t_strdup_printf("Cannot create RADOS pool %s: %s", pool.c_str(), strerror(-err));
    }
  }
  return err;
}

int RadosClusterImpl::io_ctx_create(const std::string &pool) {
  if (cluster_ref_count == 0) {
    return -ENOENT;
  }
  // pool exists? else create
  int err = pool_create(pool);
  if (err < 0) {
    return err;
  }
  err = cluster.ioctx_create(pool.c_str(), io_ctx);
  if (err < 0) {
    return err;
  }
  return 0;
}

int RadosClusterImpl::get_config_option(const char *option, std::string *value) {
  int err = cluster.conf_get(option, *value);
  if (err < 0) {
    return err;
  }
  return err;
}


