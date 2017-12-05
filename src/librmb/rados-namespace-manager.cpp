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
#include "rados-namespace-manager.h"

#include <rados/librados.hpp>

namespace librmb {

RadosNamespaceManager::~RadosNamespaceManager() {
}

bool RadosNamespaceManager::lookup_key(std::string uid, std::string *value) {
  std::string mail_namespace;

  if (uid.empty()) {
    *value = uid;
    return true;
  }

  if (!config->is_config_valid()) {
    return false;
  }

  if (!config->is_generated_namespace()) {
    *value = uid;
    return true;
  }

  if (cache.find(uid) != cache.end()) {
    *value = cache[uid];
    return true;
  }

  oid_suffix = config->get_ns_suffix();

  ceph::bufferlist bl;
  std::string oid = uid + oid_suffix;
  bool retval = false;

  // temporarily set storage namespace to config namespace
  mail_namespace = storage->get_namespace();
  storage->set_namespace(config->get_ns_cfg());
  int err = storage->read_mail(oid, &bl);
  if (err >= 0 && !bl.to_str().empty()) {
    *value = bl.to_str();
    cache[uid] = *value;
    retval = true;
  }
  // reset namespace
  storage->set_namespace(mail_namespace);
  return retval;
}

bool RadosNamespaceManager::add_namespace_entry(std::string uid, std::string value) {
  std::string mail_namespace;
  if (!config->is_config_valid()) {
    return false;
  }
  // temporarily set storage namespace to config namespace
  mail_namespace = storage->get_namespace();
  storage->set_namespace(config->get_ns_cfg());

  std::string oid = uid + oid_suffix;
  ceph::bufferlist bl;
  bl.append(value);
  bool retval = false;
  if (storage->save_mail(oid, bl) >= 0) {
    cache[uid] = value;
    retval = true;
  }
  // reset namespace
  storage->set_namespace(mail_namespace);
  return retval;
}

} /* namespace librmb */
