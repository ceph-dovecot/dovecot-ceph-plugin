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
#include <jansson.h>
#include <climits>

namespace librmb {

RadosCephConfig::RadosCephConfig(librados::IoCtx *io_ctx_) { io_ctx = io_ctx_; }

int RadosCephConfig::save_cfg() {
  ceph::bufferlist buffer;
  bool success = config.to_json(&buffer) ? save_object(config.get_cfg_object_name(), buffer) >= 0 : false;
  return success ? 0 : -1;
}

int RadosCephConfig::load_cfg() {
  if (config.is_valid()) {
    return 0;
  }
  ceph::bufferlist buffer;
  int ret = read_object(config.get_cfg_object_name(), &buffer);
  if (ret < 0) {
    return ret;
  }
  config.set_valid(true);
  return config.from_json(&buffer) ? 0 : -1;
}

bool RadosCephConfig::is_valid_key_value(std::string &key, std::string &value) {
  bool success = false;
  if (value.empty() || key.empty()) {
    return false;
  }

  if (get_config()->get_key_generated_namespace().compare(key) == 0) {
    success = value.compare("true") == 0 || value.compare("false") == 0;
  } else if (get_config()->get_key_ns_cfg().compare(key) == 0) {
    success = true;
  } else if (get_config()->get_key_ns_suffix().compare(key) == 0) {
    success = true;
  } else if (get_config()->get_key_public_namespace().compare(key) == 0) {
    success = true;
  } else if (get_config()->get_mail_attribute_key().compare(key) == 0) {
    success = true;
  } else if (get_config()->get_updateable_attribute_key().compare(key) == 0) {
    success = value.compare("true") == 0 || value.compare("false") == 0;
  } else if (get_config()->get_update_attributes_key().compare(key) == 0) {
    success = true;
  }
  return success;
}

bool RadosCephConfig::update_valid_key_value(std::string &key, std::string &value) {
  bool success = false;
  if (value.empty() || key.empty()) {
    return false;
  }
  if (get_config()->get_key_generated_namespace().compare(key) == 0) {
    get_config()->set_user_mapping(value);
    success = true;
  } else if (get_config()->get_key_ns_cfg().compare(key) == 0) {
    get_config()->set_user_ns(value);
    success = true;
  } else if (get_config()->get_key_ns_suffix().compare(key) == 0) {
    get_config()->set_user_suffix(value);
    success = true;
  } else if (get_config()->get_key_public_namespace().compare(key) == 0) {
    get_config()->set_public_namespace(value);
    success = true;
  } else if (get_config()->get_mail_attribute_key().compare(key) == 0) {
    get_config()->set_mail_attributes(value);
    success = true;
  } else if (get_config()->get_updateable_attribute_key().compare(key) == 0) {
    get_config()->set_updateable_attributes(value);
    success = true;
  } else if (get_config()->get_update_attributes_key().compare(key) == 0) {
    get_config()->set_update_attributes(value);
    success = true;
  }
  return success;
}

int RadosCephConfig::save_object(const std::string &oid, librados::bufferlist &buffer) {
  return io_ctx->write_full(oid, buffer);
}
int RadosCephConfig::read_object(const std::string &oid, librados::bufferlist *buffer) {
  size_t max = INT_MAX;
  return io_ctx->read(oid, *buffer, max, 0);
}

void RadosCephConfig::set_io_ctx_namespace(std::string &namespace_) { io_ctx->set_namespace(namespace_); }

} /* namespace librmb */
