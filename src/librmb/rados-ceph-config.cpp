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
#include <jansson.h>

namespace librmb {

RadosCephConfig::RadosCephConfig(RadosStorage *storage_) {
  storage = storage_;
  cfg_object_name = "rbox_ns_cfg";
  generated_namespace = "false";
  ns_cfg = "rbox_cfg";
  ns_suffix = "_namespace";
  is_valid = false;
}

void RadosCephConfig::create_json(ceph::bufferlist *buffer) {
  char *s = NULL;
  std::string key_generated_namespace = "generated_namespace";

  std::string key_ns_cfg = "ns_cfg";
  std::string key_ns_suffix = "ns_suffix";

  json_t *root = json_object();

  RadosMetadata attr_generated_namespace(key_generated_namespace, key_generated_namespace);
  RadosMetadata attr_ns_cfg(key_ns_cfg, ns_cfg);
  RadosMetadata attr_ns_suffix(key_ns_suffix, ns_suffix);

  json_object_set_new(root, key_generated_namespace.c_str(), json_string(generated_namespace.c_str()));
  json_object_set_new(root, key_ns_cfg.c_str(), json_string(ns_cfg.c_str()));
  json_object_set_new(root, key_ns_suffix.c_str(), json_string(ns_suffix.c_str()));

  s = json_dumps(root, 0);
  buffer->append(s);
  json_decref(root);
}
bool RadosCephConfig::load_json(ceph::bufferlist *buffer) {
  json_t *root;
  json_error_t error;
  bool ret = false;
  root = json_loads(buffer->to_str().c_str(), 0, &error);

  if (root) {
    json_t *ns = json_object_get(root, "generated_namespace");
    generated_namespace = json_string_value(ns);

    json_t *ns_cfg_ = json_object_get(root, "ns_cfg");
    ns_cfg = json_string_value(ns_cfg_);

    json_t *ns_suffix_ = json_object_get(root, "ns_suffix");
    ns_suffix = json_string_value(ns_suffix_);
    ret = true;
  }
  json_decref(root);
  return ret;
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
  ceph::bufferlist buffer;
  int ret = storage->read_mail(cfg_object_name, &buffer);
  if (ret >= 0) {
    // ok access the metadata.
    is_valid = load_json(&buffer);
  } else if (ret == -ENOENT) {
    create_json(&buffer);
    is_valid = storage->save_mail(cfg_object_name, buffer) >= 0 ? true : false;
  } else {
    // other error
    return ret;
  }
  return is_valid ? 0 : -1;
}

} /* namespace tallence */
