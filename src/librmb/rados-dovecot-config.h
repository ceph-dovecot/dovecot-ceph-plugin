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

class RadosConfig {
 public:
  RadosConfig();
  virtual ~RadosConfig();

  void update_pool_name_metadata(const char *value);

  const std::string &get_pool_name_metadata_key() { return pool_name; }

  std::map<std::string, std::string> *get_config() { return &config; }

  std::string &get_pool_name() { return config[pool_name]; }

  bool is_config_valid() { return is_valid; }
  void set_config_valid(bool is_valid_) { this->is_valid = is_valid_; }

  std::string &get_key_prefix_keywords() { return prefix_keyword; }

  std::string &get_rbox_cfg_object_name() { return config[rbox_cfg_object_name]; }

  const std::string &get_rbox_cluster_name() { return config[rbox_cluster_name]; }
  const std::string &get_rados_username() { return config[rados_username]; }
  void update_metadata(const std::string &key, const char *value_);

  void set_rbox_cfg_object_name(const std::string &value) { config[rbox_cfg_object_name] = value; }

 private:
  bool string_contains_key(const std::string &str, enum rbox_metadata_key key);

 private:
  std::map<std::string, std::string> config;
  std::string pool_name;

  std::string rbox_cfg_object_name;
  std::string rbox_cluster_name;
  std::string rados_username;
  std::string prefix_keyword;
  bool is_valid;
};

} /* namespace librmb */

#endif  // SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_
