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

#include "rados-ceph-json-config.h"
#include <jansson.h>
#include <string>
#include <sstream>

namespace librmb {

RadosCephJsonConfig::RadosCephJsonConfig() {
  // set defaults.
  cfg_object_name = "rbox_cfg";

  key_user_mapping = "user_mapping";
  key_user_ns = "user_ns";
  key_user_suffix = "user_suffix";
  key_public_namespace = "rbox_public_namespace";

  key_update_attributes = "rbox_update_attributes";
  key_mail_attributes = "rbox_mail_attributes";
  key_updateable_attributes = "rbox_updateable_attributes";

  update_attributes = "false";
  set_default_mail_attributes();
  set_default_updateable_attributes();

  user_mapping = "false";
  user_ns = "users";
  user_suffix = "_u";
  public_namespace = "public";
  valid = false;
}

void RadosCephJsonConfig::set_default_mail_attributes() {
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_MAILBOX_GUID)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_GUID)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_POP3_UIDL)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_POP3_ORDER)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_RECEIVED_TIME)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_PHYSICAL_SIZE)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_VIRTUAL_SIZE)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_ORIG_MAILBOX)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_MAIL_UID)));
  mail_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_VERSION)));
}

void RadosCephJsonConfig::set_default_updateable_attributes() {
  updateable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_ORIG_MAILBOX)));
}

bool RadosCephJsonConfig::from_json(librados::bufferlist *buffer) {
  json_t *root;
  json_error_t error;
  bool ret = false;
  root = json_loads(buffer->to_str().c_str(), 0, &error);

  if (root) {
    json_t *ns = json_object_get(root, key_user_mapping.c_str());
    user_mapping = json_string_value(ns);

    json_t *ns_cfg_ = json_object_get(root, key_user_ns.c_str());
    user_ns = json_string_value(ns_cfg_);

    json_t *ns_suffix_ = json_object_get(root, key_user_suffix.c_str());
    user_suffix = json_string_value(ns_suffix_);

    json_t *public_namespace_ = json_object_get(root, key_public_namespace.c_str());
    public_namespace = json_string_value(public_namespace_);

    json_t *mail_update_attributes_ = json_object_get(root, key_update_attributes.c_str());
    update_attributes = json_string_value(mail_update_attributes_);

    json_t *mail_attributes_ = json_object_get(root, key_mail_attributes.c_str());
    mail_attributes = json_string_value(mail_attributes_);

    json_t *updateable_attributes_ = json_object_get(root, key_updateable_attributes.c_str());
    updateable_attributes = json_string_value(updateable_attributes_);

    ret = valid = true;
    json_decref(root);
  }

  return ret;
}

bool RadosCephJsonConfig::to_json(librados::bufferlist *buffer) {
  char *s = NULL;
  json_t *root = json_object();

  json_object_set_new(root, key_user_mapping.c_str(), json_string(user_mapping.c_str()));
  json_object_set_new(root, key_user_ns.c_str(), json_string(user_ns.c_str()));
  json_object_set_new(root, key_user_suffix.c_str(), json_string(user_suffix.c_str()));
  json_object_set_new(root, key_public_namespace.c_str(), json_string(public_namespace.c_str()));

  json_object_set_new(root, key_mail_attributes.c_str(), json_string(mail_attributes.c_str()));
  json_object_set_new(root, key_updateable_attributes.c_str(), json_string(updateable_attributes.c_str()));
  json_object_set_new(root, key_update_attributes.c_str(), json_string(update_attributes.c_str()));

  s = json_dumps(root, 0);
  buffer->append(s);
  free(s);
  json_decref(root);
  return true;
}

std::string RadosCephJsonConfig::to_string() {
  std::ostringstream ss;
  ss << "Configuration : " << cfg_object_name << std::endl;
  ss << "  " << key_user_mapping << "=" << user_mapping << std::endl;
  ss << "  " << key_user_ns << "=" << user_ns << std::endl;
  ss << "  " << key_user_suffix << "=" << user_suffix << std::endl;
  ss << "  " << key_public_namespace << "=" << public_namespace << std::endl;
  ss << "  " << key_update_attributes << "=" << update_attributes << std::endl;
  ss << "  " << key_mail_attributes << "=" << mail_attributes << std::endl;
  ss << "  " << key_updateable_attributes << "=" << updateable_attributes << std::endl;
  return ss.str();
}

bool RadosCephJsonConfig::is_mail_attribute(enum rbox_metadata_key key) {
  return mail_attributes.find_first_of(static_cast<char>(key), 0) != std::string::npos;
}

bool RadosCephJsonConfig::is_updateable_attribute(enum rbox_metadata_key key) {
  return updateable_attributes.find_first_of(static_cast<char>(key), 0) != std::string::npos;
}

void RadosCephJsonConfig::update_mail_attribute(const char *value) {
  if (value == NULL) {
    return;
  }
  mail_attributes = value;
}
void RadosCephJsonConfig::update_updateable_attribute(const char *value) {
  if (value == NULL) {
    return;
  }
  updateable_attributes = value;
}

} /* namespace librmb */
