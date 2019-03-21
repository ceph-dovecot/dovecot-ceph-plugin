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

#include "rados-dictionary-impl.h"

#include <errno.h>
#include <limits.h>

#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <cstdint>

#include <rados/librados.hpp>

using std::string;
using std::stringstream;
using std::map;
using std::pair;
using std::set;

using librmb::RadosDictionaryImpl;

#define DICT_USERNAME_SEPARATOR '/'
#define DICT_PATH_PRIVATE "priv/"
#define DICT_PATH_SHARED "shared/"

RadosDictionaryImpl::RadosDictionaryImpl(RadosCluster *_cluster, const string &_poolname, const string &_username,
                                         const string &_oid, librmb::RadosGuidGenerator *guid_generator_,
                                         const std::string &cfg_object_name_)
    : cluster(_cluster),
      poolname(_poolname),
      username(_username),
      oid(_oid),
      cfg(nullptr),
      namespace_mgr(nullptr),
      cfg_object_name(cfg_object_name_) {
  shared_io_ctx_created = false;
  private_io_ctx_created = false;
  guid_generator = guid_generator_;
  shared_oid = oid;
  private_oid = oid;
}

RadosDictionaryImpl::~RadosDictionaryImpl() {
  if (namespace_mgr != nullptr) {
    delete namespace_mgr;
    namespace_mgr = nullptr;
  }
  if (cfg != nullptr) {
    delete cfg;
    cfg = nullptr;
  }
}

const string RadosDictionaryImpl::get_shared_oid() { return shared_oid; }

const string RadosDictionaryImpl::get_private_oid() { return private_oid; }

const string RadosDictionaryImpl::get_full_oid(const std::string &key) {
  if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
    return get_private_oid();
  } else if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
    return get_shared_oid();
  } else {
    // TODO(peter) i_unreached();
  }
  return "";
}

librados::IoCtx &RadosDictionaryImpl::get_shared_io_ctx() {
  if (!shared_io_ctx_created) {
    shared_io_ctx_created = cluster->io_ctx_create(poolname, &shared_io_ctx) == 0;
    std::string ns;
    if (load_configuration(&shared_io_ctx)) {
      std::string user = cfg->get_public_namespace();
      lookup_namespace(user, cfg, &ns);
      shared_io_ctx.set_namespace(ns);
    }
  }
  return shared_io_ctx;
}

bool RadosDictionaryImpl::load_configuration(librados::IoCtx *io_ctx) {
  bool loaded = true;
  if (cfg != nullptr) {
    return loaded;
  }
  try {
    cfg = new librmb::RadosDovecotCephCfgImpl(io_ctx);
  } catch (std::bad_alloc &bd) {
    return false;
  }
  cfg->set_rbox_cfg_object_name(cfg_object_name);
  cfg->set_config_valid(true);
  int load_cfg = cfg->load_rados_config();
  if (load_cfg == -ENOENT) {
    cfg->save_default_rados_config();
  } else if (load_cfg < 0) {
    // error
    loaded = false;
  }

  if (username.empty()) {
    username = cfg->get_public_namespace();
  }
  return loaded;
}

bool RadosDictionaryImpl::lookup_namespace(std::string &username_, librmb::RadosDovecotCephCfg *cfg_, std::string *ns) {
  if (namespace_mgr == nullptr) {
    try {
      namespace_mgr = new librmb::RadosNamespaceManager(cfg_);
    } catch (std::bad_alloc &bd) {
      return false;
    }
  }
  if (!namespace_mgr->lookup_key(username_, ns)) {
    return namespace_mgr->add_namespace_entry(username_, ns, guid_generator) ? true : false;
  }
  return 0;
}

librados::IoCtx &RadosDictionaryImpl::get_private_io_ctx() {
  if (!private_io_ctx_created) {
    if (cluster->io_ctx_create(poolname, &private_io_ctx) == 0) {
      if (load_configuration(&private_io_ctx)) {
        std::string ns;
        std::string user = username + cfg->get_user_suffix();
        lookup_namespace(user, cfg, &ns);
        private_io_ctx_created = true;
        private_io_ctx.set_namespace(ns);
      }
    }
  }
  return private_io_ctx;
}

librados::IoCtx &RadosDictionaryImpl::get_io_ctx(const std::string &key) {
  if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
    return get_private_io_ctx();
  } else if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
    return get_shared_io_ctx();
  }
  assert(false);  // TODO(jrse): in the unlikely case (it's either private or public), io_ctx is not private and not
                  // public, the return value is
                  // undefinied in release build!
}

int RadosDictionaryImpl::get(const string &key, string *value_r) {
  int r_val = -1;

  set<string> keys;
  keys.insert(key);

  map<std::string, librados::bufferlist> map;
  librados::ObjectReadOperation oro;
  oro.omap_get_vals_by_keys(keys, &map, &r_val);

  librados::bufferlist bl;
  int err = get_io_ctx(key).operate(get_full_oid(key), &oro, &bl);

  if (err == 0) {
    if (r_val == 0) {
      auto it = map.find(key);  // map.begin();
      if (it != map.end()) {
        *value_r = it->second.to_str();
        return 0;
      }
      return -ENOENT;
    } else {
      err = r_val;
    }
  }

  return err;
}

void RadosDictionaryImpl::remove_completion(librados::AioCompletion *c) {
  completions_mutex.lock();
  completions.remove(c);
  completions_mutex.unlock();
}
void RadosDictionaryImpl::push_back_completion(librados::AioCompletion *c) {
  completions_mutex.lock();
  completions.push_back(c);
  completions_mutex.unlock();
}

void RadosDictionaryImpl::wait_for_completions() {
  while (!completions.empty()) {
    auto c = completions.front();
    c->wait_for_complete_and_cb();
    remove_completion(c);
    c->release();
  }
}
