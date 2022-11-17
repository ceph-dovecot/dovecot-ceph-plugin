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

#include "rados-cluster-impl.h"

#include <list>
#include <string>
#include <utility>

#include "rados-dictionary-impl.h"
#include "rados-storage-impl.h"
#include "rados-util.h"
using std::list;
using std::pair;
using std::string;

using librmb::RadosClusterImpl;

const char *RadosClusterImpl::CLIENT_MOUNT_TIMEOUT = "client_mount_timeout";
const char *RadosClusterImpl::RADOS_MON_OP_TIMEOUT = "rados_mon_op_timeout";
const char *RadosClusterImpl::RADOS_OSD_OP_TIMEOUT = "rados_osd_op_timeout";

const char *RadosClusterImpl::CLIENT_MOUNT_TIMEOUT_DEFAULT = "600";
const char *RadosClusterImpl::RADOS_MON_OP_TIMEOUT_DEFAULT = "600";
const char *RadosClusterImpl::RADOS_OSD_OP_TIMEOUT_DEFAULT = "600";

// Note: Using Dictionary und RadosStorage with different ceph cluster / user is currently
//       not supported.
librados::Rados *RadosClusterImpl::cluster = 0;
int RadosClusterImpl::cluster_ref_count = 0;
bool RadosClusterImpl::connected = false;

RadosClusterImpl::RadosClusterImpl() {}

RadosClusterImpl::~RadosClusterImpl() {}

int RadosClusterImpl::init() {
  int ret = 0;
  if (RadosClusterImpl::cluster_ref_count == 0) {
    RadosClusterImpl::cluster = new librados::Rados();
    ret = RadosClusterImpl::cluster->init(nullptr);
    if (ret == 0) {
      ret = initialize();
    }
  }
  if (ret == 0)
    RadosClusterImpl::cluster_ref_count++;
  return ret;
}

int RadosClusterImpl::init(const std::string &clustername, const std::string &rados_username) {
  int ret = 0;
  if (RadosClusterImpl::cluster_ref_count == 0) {
    RadosClusterImpl::cluster = new librados::Rados();

    ret = RadosClusterImpl::cluster->init2(rados_username.c_str(), clustername.c_str(), 0);
    if (ret == 0) {
      ret = initialize();
    }
  }
  if (ret == 0)
    RadosClusterImpl::cluster_ref_count++;

  return ret;
}


std::vector<std::string> RadosClusterImpl::list_pgs_for_pool(std::string &pool_name) {
    std::cout << " ola "  << RadosClusterImpl::cluster << std::endl;
    
    if(is_connected()){
      std::cout << " is connected YES" << std::endl;
    }else{
      std::cout << " is connected NO" << std::endl;
      connect();
    }

    const string cmd =
    "{"
    "\"prefix\": \"pg ls-by-pool\", "
    "\"poolstr\": \"" + pool_name + "\""
    "}";      
    
    std::cout << "cmd: " << cmd << std::endl;
    
    librados::bufferlist inbl;
    librados::bufferlist outbl;
    int res = RadosClusterImpl::cluster->mon_command(cmd, inbl, &outbl, nullptr);
    std::cout << "inbl command " << inbl  <<std::endl;
    std::cout << "outbl command " << outbl.c_str()  <<std::endl;

    std::vector<std::string> list = RadosUtils::extractPgs(std::string(outbl.c_str()));
  
    for (auto const &token: list) {
          std::cout << token << std::endl;        
    }
    return list;
}

std::map<std::string, std::vector<std::string>> RadosClusterImpl::list_pgs_osd_for_pool(std::string &pool_name) {    
    
    if(!is_connected()){      
      connect();
    }
    
    const string cmd =
    "{"
    "\"prefix\": \"pg ls-by-pool\", "
    "\"poolstr\": \"" + pool_name + "\""
    "}";      
        
    librados::bufferlist inbl;
    librados::bufferlist outbl;
    RadosClusterImpl::cluster->mon_command(cmd, inbl, &outbl, nullptr);
    return RadosUtils::extractPgAndPrimaryOsd(std::string(outbl.c_str()));
}
int RadosClusterImpl::initialize() {
  int ret = 0;

  ret = RadosClusterImpl::cluster->conf_parse_env(nullptr);

  if (ret == 0) {
    ret = RadosClusterImpl::cluster->conf_read_file(nullptr);
  }
  // check if ceph configuration has connection timeout set, else set defaults to avoid
  // waiting forever
  std::string cfg_value;
  if (get_config_option(CLIENT_MOUNT_TIMEOUT, &cfg_value) < 0) {
    RadosClusterImpl::cluster->conf_set(CLIENT_MOUNT_TIMEOUT, CLIENT_MOUNT_TIMEOUT_DEFAULT);
  }
  ret = get_config_option(RADOS_MON_OP_TIMEOUT, &cfg_value);
  if (ret < 0 || cfg_value.compare("0") == 0) {
    RadosClusterImpl::cluster->conf_set(RADOS_MON_OP_TIMEOUT, RADOS_MON_OP_TIMEOUT_DEFAULT);
  }
  ret = get_config_option(RADOS_OSD_OP_TIMEOUT, &cfg_value);
  if (ret < 0 || cfg_value.compare("0") == 0) {
    RadosClusterImpl::cluster->conf_set(RADOS_OSD_OP_TIMEOUT, RADOS_OSD_OP_TIMEOUT_DEFAULT);
  }

  for (std::map<const char *, const char *>::iterator it = client_options.begin(); it != client_options.end(); ++it) {
    RadosClusterImpl::cluster->conf_set(it->first, it->second);
  }
  return ret;
}

bool RadosClusterImpl::is_connected() { return RadosClusterImpl::connected; }

int RadosClusterImpl::connect() {
  int ret = 0;
  if (RadosClusterImpl::cluster_ref_count > 0 && !RadosClusterImpl::connected) {
    ret = RadosClusterImpl::cluster->connect();
    RadosClusterImpl::connected = (ret == 0);
  }
  return ret;
}

void RadosClusterImpl::deinit() {
  if (RadosClusterImpl::cluster_ref_count > 0) {
    if (--RadosClusterImpl::cluster_ref_count == 0) {
      if (RadosClusterImpl::connected) {
        RadosClusterImpl::cluster->shutdown();
        RadosClusterImpl::connected = false;
        delete RadosClusterImpl::cluster;
        RadosClusterImpl::cluster = nullptr;
      }
    }
  }
}

int RadosClusterImpl::pool_create(const string &pool) {
  // pool exists? else create

  int ret = connect();
  if (ret < 0) {
    return ret;
  }

  list<pair<int64_t, string>> pool_list;
  ret = RadosClusterImpl::cluster->pool_list2(pool_list);
  if (ret < 0) {
    return ret;
  }

  bool pool_found = false;
  for (list<pair<int64_t, string>>::iterator it = pool_list.begin(); it != pool_list.end(); ++it) {
    if ((*it).second.compare(pool) == 0) {
      pool_found = true;
      break;
    }
  }
  if (pool_found != true) {
    ret = RadosClusterImpl::cluster->pool_create(pool.c_str());
    pool_found = (ret == 0);
  }

  return ret;
}

int RadosClusterImpl::io_ctx_create(const string &pool, librados::IoCtx *io_ctx) {
  int ret = 0;

  assert(io_ctx != nullptr);

  if (RadosClusterImpl::cluster_ref_count == 0) {
    ret = -ENOENT;
    return ret;
  }

  ret = connect();
  if (ret < 0) {
    return ret;
  }

  // pool exists? else create
  ret = pool_create(pool);
  if (ret == 0) {
    ret = RadosClusterImpl::cluster->ioctx_create(pool.c_str(), *io_ctx);
  }
  return ret;
}
int RadosClusterImpl::recovery_index_io_ctx(const std::string &pool, 
  librados::IoCtx *io_ctx) {
    if(!is_connected()) {
      return -1;
    }
    // pool exists? else create
    int ret = pool_create(pool);
    if (ret == 0) {
      ret = RadosClusterImpl::cluster->ioctx_create(pool.c_str(), *io_ctx);
    }
    return ret;      
}

int RadosClusterImpl::get_config_option(const char *option, string *value) {
  return RadosClusterImpl::cluster->conf_get(option, *value);
}

void RadosClusterImpl::set_config_option(const char *option, const char *value) { client_options[option] = value; }
