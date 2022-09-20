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

#ifndef SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_
#define SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_

#include <map>
#include <string>

#include "rados-types.h"

namespace librmb {
/**
 * The Rados Config
 * class holds and provides access to all configuration values which
 * are stored in ceph.
 *
 */
class RadosConfig {
 public:
  RadosConfig();
  virtual ~RadosConfig();

  void update_pool_name_metadata(const char *value);

  const std::string &get_pool_name_metadata_key() { return pool_name; }

  std::map<std::string, std::string> *get_config() { return &config; }

  std::string &get_pool_name() { return config[pool_name]; }
  const std::string &get_rados_save_log_file() { return config[save_log]; }
  bool is_config_valid() { return is_valid; }
  void set_config_valid(bool is_valid_) { this->is_valid = is_valid_; }

  std::string &get_key_prefix_keywords() { return prefix_keyword; }

  std::string &get_rbox_cfg_object_name() { return config[rbox_cfg_object_name]; }

  const std::string &get_write_method() { return config[rbox_write_method]; }
  const std::string &get_chunk_size() { return config[rbox_chunk_size]; }

  const std::string &get_rbox_cluster_name() { return config[rbox_cluster_name]; }
  const std::string &get_rados_username() { return config[rados_username]; }
  
  const std::string &get_object_search_method()  { return config[rbox_object_search_method]; }
  const std::string &get_object_search_threads() { return config[rbox_object_search_threads]; }

  void update_metadata(const std::string &key, const char *value_);
  bool is_ceph_posix_bugfix_enabled() {
    return config[bugfix_cephfs_posix_hardlinks].compare("true") == 0 ? true : false;
  }
  void set_rbox_cfg_object_name(const std::string &value) { config[rbox_cfg_object_name] = value; }
  bool is_rbox_check_empty_mailboxes() {
    return config[rbox_check_empty_mailboxes].compare("true") == 0 ? true : false;
  }
  bool is_ceph_aio_wait_for_safe_and_cb() {
    return config[rbox_ceph_aio_wait_for_safe_and_cb].compare("true") == 0 ? true : false;
  }
  bool is_write_chunks() {
    return config[rbox_ceph_write_chunks].compare("true") == 0 ? true : false;
  }

  /*!
   * print configuration
   */
  std::string to_string();

 private:
  bool string_contains_key(const std::string &str, enum rbox_metadata_key key);

 private:
  std::map<std::string, std::string> config;
  std::string pool_name;

  std::string rbox_cfg_object_name;
  std::string rbox_cluster_name;
  std::string rados_username;
  std::string prefix_keyword;
  std::string bugfix_cephfs_posix_hardlinks;
  std::string save_log;
  std::string rbox_check_empty_mailboxes;
  std::string rbox_ceph_aio_wait_for_safe_and_cb;
  std::string rbox_ceph_write_chunks;
  std::string rbox_chunk_size;
  std::string rbox_write_method;
  std::string rbox_object_search_method;
  std::string rbox_object_search_threads;
  bool is_valid;
};

} /* namespace librmb */

#endif  // SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_
