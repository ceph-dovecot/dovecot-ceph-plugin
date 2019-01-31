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

#include <iostream>
#include <sstream>
#include "rados-types.h"

namespace librmb {

RadosConfig::RadosConfig()
    : pool_name("rbox_pool_name"),
      rbox_cfg_object_name("rbox_cfg_object_name"),
      rbox_cluster_name("rbox_cluster_name"),
      rados_username("rados_user_name"),
      prefix_keyword("k"),
      bugfix_cephfs_posix_hardlinks("rbox_bugfix_cephfs_21652"),
      save_log("rados_save_log"),
      rbox_check_empty_mailboxes("rados_check_empty_mailboxes"),
      rbox_ceph_aio_wait_for_safe_and_cb("rbox_ceph_aio_wait_for_safe_and_cb"),
      rbox_ceph_write_ops_in_save_continue("rbox_ceph_write_ops_in_save_continue") {
  config[pool_name] = "mail_storage";

  config[rbox_cfg_object_name] = "rbox_cfg";
  config[rbox_cluster_name] = "ceph";
  config[rados_username] = "client.admin";
  config[bugfix_cephfs_posix_hardlinks] = "false";
  config[save_log] = "";
  config[rbox_check_empty_mailboxes] = "false";
  config[rbox_ceph_aio_wait_for_safe_and_cb] = "false";
  config[rbox_ceph_write_ops_in_save_continue] = "false";
  is_valid = false;
}

bool RadosConfig::string_contains_key(const std::string &str, enum rbox_metadata_key key) {
  std::string value(librmb::rbox_metadata_key_to_char(key));
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

std::string RadosConfig::to_string() {
  std::stringstream ss;
  ss << "Dovecot configuration: (90-plugin.conf)" << std::endl;
  ss << "  " << rbox_cfg_object_name << "=" << config[rbox_cfg_object_name] << std::endl;
  ss << "  " << rbox_cluster_name << "=" << config[rbox_cluster_name] << std::endl;
  ss << "  " << rados_username << "=" << config[rados_username] << std::endl;
  ss << "  " << bugfix_cephfs_posix_hardlinks << "=" << config[bugfix_cephfs_posix_hardlinks] << std::endl;
  ss << "  " << save_log << "=" << config[save_log] << std::endl;
  ss << "  " << rbox_check_empty_mailboxes << "=" << config[rbox_check_empty_mailboxes] << std::endl;
  ss << "  " << rbox_ceph_aio_wait_for_safe_and_cb << "=" << config[rbox_ceph_aio_wait_for_safe_and_cb] << std::endl;
  ss << "  " << rbox_ceph_write_ops_in_save_continue << "=" << config[rbox_ceph_write_ops_in_save_continue]
     << std::endl;
  return ss.str();
}

RadosConfig::~RadosConfig() {}

} /* namespace librmb */
