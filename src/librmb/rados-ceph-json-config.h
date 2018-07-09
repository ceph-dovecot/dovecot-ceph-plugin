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

#ifndef SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_
#define SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_

#include <string>

#include <rados/librados.hpp>
#include "rados-types.h"

namespace librmb {

class RadosCephJsonConfig {
 public:
  RadosCephJsonConfig();
  virtual ~RadosCephJsonConfig() {}

  bool from_json(librados::bufferlist* buffer);
  bool to_json(librados::bufferlist* buffer);
  std::string to_string();

  const std::string& get_cfg_object_name() const { return cfg_object_name; }

  void set_cfg_object_name(const std::string& cfgObjectName) { cfg_object_name = cfgObjectName; }

  const std::string& get_user_mapping() const { return user_mapping; }

  void set_user_mapping(const std::string& user_mapping_) { user_mapping = user_mapping_; }

  bool is_valid() const { return valid; }

  void set_valid(bool isValid) { valid = isValid; }

  std::string& get_user_ns() { return user_ns; }

  void set_user_ns(const std::string& user_ns_) { user_ns = user_ns_; }

  std::string& get_user_suffix() { return user_suffix; }

  void set_user_suffix(const std::string& user_suffix_) { user_suffix = user_suffix_; }

  const std::string& get_public_namespace() const { return public_namespace; }

  void set_public_namespace(const std::string& public_namespace_) { public_namespace = public_namespace_; }

  void set_mail_attributes(const std::string& mail_attributes_) { mail_attributes = mail_attributes_; }
  void set_update_attributes(const std::string& update_attributes_) { update_attributes = update_attributes_; }
  void set_updateable_attributes(const std::string& updateable_attributes_) {
    updateable_attributes = updateable_attributes_;
  }

  bool is_mail_attribute(enum rbox_metadata_key key);
  bool is_updateable_attribute(enum rbox_metadata_key key);
  bool is_update_attributes() { return update_attributes.compare("true") == 0; }

  void set_metadata_storage_module(const std::string& metadata_storage_module_) {
    metadata_storage_module = metadata_storage_module_;
  }
  const std::string& get_metadata_storage_module() { return metadata_storage_module; }

  void set_metadata_storage_attribute(const std::string& metadata_storage_attribute_) {
    metadata_storage_attribute = metadata_storage_attribute_;
  }
  const std::string& get_metadata_storage_attribute() { return metadata_storage_attribute; }

  void update_mail_attribute(const char* value);
  void update_updateable_attribute(const char* value);

  const std::string& get_key_user_mapping() const { return key_user_mapping; }
  const std::string& get_key_ns_cfg() const { return key_user_ns; }
  const std::string& get_key_ns_suffix() const { return key_user_suffix; }
  const std::string& get_key_public_namespace() const { return key_public_namespace; }

  const std::string& get_mail_attribute_key() { return key_mail_attributes; }
  const std::string& get_updateable_attribute_key() { return key_updateable_attributes; }
  const std::string& get_update_attributes_key() { return key_update_attributes; }

  const std::string& get_metadata_storage_module_key() { return key_metadata_storage_module; }
  const std::string& get_metadata_storage_attribute_key() { return key_metadata_storage_attribute; }

 private:
  void set_default_mail_attributes();
  void set_default_updateable_attributes();

 private:
  std::string cfg_object_name;
  bool valid;
  std::string user_mapping;
  std::string user_ns;
  std::string user_suffix;
  std::string public_namespace;

  std::string mail_attributes;
  std::string update_attributes;
  std::string updateable_attributes;

  std::string metadata_storage_module;
  std::string metadata_storage_attribute;

  std::string key_user_mapping;
  std::string key_user_ns;
  std::string key_user_suffix;
  std::string key_public_namespace;

  std::string key_mail_attributes;
  std::string key_update_attributes;
  std::string key_updateable_attributes;

  std::string key_metadata_storage_module;
  std::string key_metadata_storage_attribute;
};

} /* namespace librmb */

#endif  // SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_
