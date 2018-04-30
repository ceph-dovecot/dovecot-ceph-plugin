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

#include "rados-dovecot-config.h"

namespace librmb {

std::string pool_name;

RadosConfig::RadosConfig()
    : pool_name("rbox_pool_name"),
      rbox_cfg_object_name("rbox_cfg_object_name"),
      rbox_cluster_name("rbox_cluster_name"),
      rados_username("rados_user_name"),
      prefix_keyword("k"),
      bugfix_cephfs_posix_hardlinks("rbox_bugfix_cephfs_21652") {
  config[pool_name] = "mail_storage";

  config[rbox_cfg_object_name] = "rbox_cfg";
  config[rbox_cluster_name] = "ceph";
  config[rados_username] = "client.admin";
  config[bugfix_cephfs_posix_hardlinks] = "false";
  is_valid = false;
}

bool RadosConfig::string_contains_key(const std::string &str, enum rbox_metadata_key key) {
  std::string value(1, static_cast<char>(key));
  return str.find(value) != std::string::npos;
}

void RadosConfig::update_pool_name_metadata(const char *value) {
  if (value == NULL) {
    return;
  }
  config[pool_name] = value;
}
void RadosConfig::update_metadata(const std::string &key, const char *value_) {
  if (value_ == NULL) {
    return;
  }
  if (config.find(key) != config.end()) {
    std::string value = value_;
    config[key] = value;
  }
}

RadosConfig::~RadosConfig() {}

} /* namespace librmb */
