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
#ifndef SRC_LIBRMB_RADOS_CEPH_CONFIG_H_
#define SRC_LIBRMB_RADOS_CEPH_CONFIG_H_

#include "rados-storage.h"
#include "rados-ceph-json-config.h"

namespace librmb {

class RadosCephConfig {
 public:
  RadosCephConfig(RadosStorage *storage_);
  virtual ~RadosCephConfig() {}

  // load settings from rados cfg_object
  int load_cfg();
  int save_cfg();

  void set_storage(RadosStorage *storage_) { storage = storage_; }
  bool is_config_valid() { return config.is_valid(); }
  void set_config_valid(bool valid_) { config.set_valid(valid_); }
  bool is_generated_namespace() { return !config.get_generated_namespace().compare("true"); }
  void set_generated_namespace(bool value_) { config.set_generated_namespace(value_ ? "true" : "false"); }
  void set_ns_cfg(std::string &ns_) { config.set_ns_cfg(ns_); }
  std::string get_ns_cfg() { return config.get_ns_cfg(); }
  void set_ns_suffix(std::string &ns_suffix_) { config.set_ns_suffix(ns_suffix_); }
  std::string get_ns_suffix() { return config.get_ns_suffix(); }
  const std::string &get_public_namespace() const { return config.get_public_namespace(); }
  void set_public_namespace(std::string &public_namespace_) { config.set_public_namespace(public_namespace_); }

  void set_cfg_object_name(std::string cfg_object_name_) { config.set_cfg_object_name(cfg_object_name_); }
  std::string get_cfg_object_name() { return config.get_cfg_object_name(); }
  RadosCephJsonConfig *get_config() { return &config; }

 private:
  RadosCephJsonConfig config;
  RadosStorage *storage;
};

} /* namespace tallence */

#endif /* SRC_LIBRMB_RADOS_CEPH_CONFIG_H_ */
