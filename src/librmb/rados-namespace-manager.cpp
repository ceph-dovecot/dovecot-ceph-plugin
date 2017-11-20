/*
 * rados-namespace-manager.cpp
 *
 *  Created on: Nov 17, 2017
 *      Author: jan
 */

#include "rados-namespace-manager.h"

#include <rados/librados.hpp>

namespace librmb {

RadosNamespaceManager::~RadosNamespaceManager() {
}

bool RadosNamespaceManager::lookup_key(std::string uid, std::string *value) {
  if (uid.empty()) {
    *value = uid;
    return true;
  }

  if (!storage->get_rados_config()->is_config_valid()) {
    return false;
  }

  if (!storage->get_rados_config()->is_generated_namespace()) {
    *value = uid;
    return true;
  }

  if (cache.find(uid) != cache.end()) {
    *value = cache[uid];
    return true;
  }

  std::set<string> keys;
  keys.insert(uid);
  ceph::bufferlist bl;
  std::string oid = uid + oid_suffix;
  int read_len = 4194304;

  int err = storage->get_io_ctx().read(oid, bl, read_len, 0);
  if (err < 0) {
    return false;
  }
  if (bl.to_str().empty()) {
    return false;
  }

  *value = bl.to_str();
  cache[uid] = *value;
  return true;
}

bool RadosNamespaceManager::add_namespace_entry(std::string uid, std::string value) {
  if (!storage->get_rados_config()->is_config_valid()) {
    return false;
  }

  std::string oid = uid + oid_suffix;
  ceph::bufferlist bl;
  bl.append(value);
  if (storage->get_io_ctx().write_full(oid, bl) != 0) {
    return false;
  }
  cache[uid] = value;
  return true;
}

} /* namespace librmb */
