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

#include "rados-ceph-config.h"
#include <errno.h>

namespace librmb {

RadosCephConfig::RadosCephConfig(RadosStorage *storage_) {
  storage = storage_;
  cfg_object_name = "rbox_cfg";
  generated_namespace = "false";
  ns_cfg = "rbox_cfg";
  ns_suffix = "_namespace";
  is_valid = false;
}


int RadosCephConfig::load_cfg() {
  if (is_valid) {
    return 0;
  }
  std::string key_generated_namespace = "generated_namespace";

  std::string key_ns_cfg = "ns_cfg";
  std::string key_ns_suffix = "ns_suffix";
  std::set<std::string> keys;
  keys.insert(key_generated_namespace);
  keys.insert(key_ns_cfg);
  keys.insert(key_ns_suffix);

  std::map<std::string, ceph::bufferlist> metadata;
  int ret = storage->load_extended_metadata(cfg_object_name, keys, &metadata);
  if (ret >= 0) {
    // ok access the metadata.
    try {
      generated_namespace = metadata[key_generated_namespace].to_str();
      ns_cfg = metadata[key_ns_cfg].to_str();
      ns_suffix = metadata[key_ns_suffix].to_str();
      is_valid = true;
    } catch (std::exception &e) {
      is_valid = false;
    }
  } else if (ret == -ENOENT) {
    // create
    std::list<librmb::RadosMetadata> cfg;
    RadosMetadata attr_generated_namespace(key_generated_namespace, generated_namespace);
    RadosMetadata attr_ns_cfg(key_ns_cfg, ns_cfg);
    RadosMetadata attr_ns_suffix(key_ns_suffix, ns_suffix);
    cfg.push_back(attr_generated_namespace);
    cfg.push_back(attr_ns_cfg);
    cfg.push_back(attr_ns_suffix);
    is_valid = storage->update_metadata(cfg_object_name, cfg);
  } else {
    // other error
    return ret;
  }
  return is_valid ? 0 : -1;
}

} /* namespace tallence */
