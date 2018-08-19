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
#ifndef SRC_LIBRMB_RADOS_CEPH_CONFIG_H_
#define SRC_LIBRMB_RADOS_CEPH_CONFIG_H_

#include <string>
#include "rados-ceph-json-config.h"
#include "rados-types.h"
#include <rados/librados.hpp>
#include "rados-storage.h"

namespace librmb {

class RadosCephConfig {
 public:
  explicit RadosCephConfig(librados::IoCtx *io_ctx_);
  RadosCephConfig() { io_ctx = nullptr; }
  virtual ~RadosCephConfig() {}

  // load settings from rados cfg_object
  int load_cfg();
  int save_cfg();

  void set_io_ctx(librados::IoCtx *io_ctx_) { io_ctx = io_ctx_; }
  bool is_config_valid() { return config.is_valid(); }
  void set_config_valid(bool valid_) { config.set_valid(valid_); }
  bool is_user_mapping() { return !config.get_user_mapping().compare("true"); }
  void set_user_mapping(bool value_) { config.set_user_mapping(value_ ? "true" : "false"); }
  void set_user_ns(const std::string &ns_) { config.set_user_ns(ns_); }
  std::string &get_user_ns() { return config.get_user_ns(); }
  void set_user_suffix(const std::string &ns_suffix_) { config.set_user_suffix(ns_suffix_); }
  std::string &get_user_suffix() { return config.get_user_suffix(); }
  const std::string &get_public_namespace() const { return config.get_public_namespace(); }
  void set_public_namespace(const std::string &public_namespace_) { config.set_public_namespace(public_namespace_); }

  void set_cfg_object_name(const std::string &cfg_object_name_) { config.set_cfg_object_name(cfg_object_name_); }
  std::string get_cfg_object_name() { return config.get_cfg_object_name(); }
  RadosCephJsonConfig *get_config() { return &config; }

  bool is_valid_key_value(const std::string &key, const std::string &value);
  bool update_valid_key_value(const std::string &key, const std::string &value);

  bool is_mail_attribute(enum rbox_metadata_key key) { return config.is_mail_attribute(key); }
  bool is_updateable_attribute(enum rbox_metadata_key key) { return config.is_updateable_attribute(key); }
  bool is_update_attributes() { return config.is_update_attributes(); }
  void set_update_attributes(const std::string &update_attributes_) {
    config.set_update_attributes(update_attributes_);
  }

  void update_mail_attribute(const char *value) { config.update_mail_attribute(value); }
  void update_updateable_attribute(const char *value) { config.update_updateable_attribute(value); }

  const std::string &get_metadata_storage_module() { return config.get_metadata_storage_module(); }
  const std::string &get_metadata_storage_attribute() { return config.get_metadata_storage_attribute(); }


  const std::string &get_mail_attribute_key() { return config.get_mail_attribute_key(); }
  const std::string &get_updateable_attribute_key() { return config.get_updateable_attribute_key(); }
  const std::string &get_update_attributes_key() { return config.get_update_attributes_key(); }

  int save_object(const std::string &oid, librados::bufferlist &buffer);
  int read_object(const std::string &oid, librados::bufferlist *buffer);
  void set_io_ctx_namespace(const std::string &namespace_);

 private:
  RadosCephJsonConfig config;
  librados::IoCtx *io_ctx;
};

} /* namespace tallence */

#endif /* SRC_LIBRMB_RADOS_CEPH_CONFIG_H_ */
