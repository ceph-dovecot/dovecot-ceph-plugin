/*
 * rados-ceph-Json-config.cpp
 *
 *  Created on: Nov 28, 2017
 *      Author: jan
 */

#include "rados-ceph-json-config.h"
#include <jansson.h>
#include <string>
#include <sstream>
namespace librmb {
RadosCephJsonConfig::RadosCephJsonConfig() {
  // set defaults.
  cfg_object_name = "rbox_cfg";

  key_generated_namespace = "generated_namespace";
  key_ns_cfg = "ns_cfg";
  key_ns_suffix = "ns_suffix";
  key_public_namespace = "public_namespace";

  key_update_attributes = "rbox_update_attributes";
  key_mail_attributes = "rbox_mail_attributes";
  key_updateable_attributes = "rbox_updateable_attributes";

  update_attributes = "false";
  mail_attributes = set_default_mail_attributes();
  updateable_attributes = set_default_updateable_attributes();

  generated_namespace = "false";
  ns_cfg = "rbox_ns_cfg";
  ns_suffix = "_namespace";
  public_namespace = "public";
  valid = false;
}

std::string RadosCephJsonConfig::set_default_mail_attributes() {
  std::string mutable_attributes;
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_MAILBOX_GUID)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_GUID)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_POP3_UIDL)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_POP3_ORDER)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_RECEIVED_TIME)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_PHYSICAL_SIZE)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_VIRTUAL_SIZE)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_ORIG_MAILBOX)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_MAIL_UID)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_VERSION)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_OLDV1_KEYWORDS)));
  return mutable_attributes;
}

std::string RadosCephJsonConfig::set_default_updateable_attributes() {
  std::string mutable_attributes;
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_ORIG_MAILBOX)));
  return mutable_attributes;
}

bool RadosCephJsonConfig::from_json(librados::bufferlist *buffer) {
  json_t *root;
  json_error_t error;
  bool ret = false;
  root = json_loads(buffer->to_str().c_str(), 0, &error);

  if (root) {
    json_t *ns = json_object_get(root, key_generated_namespace.c_str());
    generated_namespace = json_string_value(ns);

    json_t *ns_cfg_ = json_object_get(root, key_ns_cfg.c_str());
    ns_cfg = json_string_value(ns_cfg_);

    json_t *ns_suffix_ = json_object_get(root, key_ns_suffix.c_str());
    ns_suffix = json_string_value(ns_suffix_);

    json_t *public_namespace_ = json_object_get(root, key_public_namespace.c_str());
    public_namespace = json_string_value(public_namespace_);

    json_t *mail_update_attributes_ = json_object_get(root, key_update_attributes.c_str());
    updateable_attributes = json_string_value(mail_update_attributes_);

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

  json_object_set_new(root, key_generated_namespace.c_str(), json_string(generated_namespace.c_str()));
  json_object_set_new(root, key_ns_cfg.c_str(), json_string(ns_cfg.c_str()));
  json_object_set_new(root, key_ns_suffix.c_str(), json_string(ns_suffix.c_str()));
  json_object_set_new(root, key_public_namespace.c_str(), json_string(public_namespace.c_str()));

  json_object_set_new(root, key_mail_attributes.c_str(), json_string(mail_attributes.c_str()));
  json_object_set_new(root, key_updateable_attributes.c_str(), json_string(updateable_attributes.c_str()));
  json_object_set_new(root, key_update_attributes.c_str(), json_string(update_attributes.c_str()));

  s = json_dumps(root, 0);
  buffer->append(s);
  json_decref(root);
  return true;
}

std::string RadosCephJsonConfig::to_string() {
  std::ostringstream ss;
  ss << "Configuration : " << cfg_object_name << std::endl;
  ss << "  " << key_generated_namespace << "=" << generated_namespace << std::endl;
  ss << "  " << key_ns_cfg << "=" << ns_cfg << std::endl;
  ss << "  " << key_ns_suffix << "=" << ns_suffix << std::endl;
  ss << "  " << key_public_namespace << "=" << public_namespace << std::endl;
  ss << "  " << key_update_attributes << "=" << update_attributes << std::endl;
  ss << "  " << key_mail_attributes << "=" << mail_attributes << std::endl;
  ss << "  " << key_updateable_attributes << "=" << updateable_attributes << std::endl;
  return ss.str();
}

bool RadosCephJsonConfig::is_mail_attribute(enum rbox_metadata_key key) {
  return string_contains_key(mail_attributes, key);
}

bool RadosCephJsonConfig::is_updateable_attribute(enum rbox_metadata_key key) {
  return string_contains_key(updateable_attributes, key);
}

bool RadosCephJsonConfig::string_contains_key(std::string &str, enum rbox_metadata_key key) {
  std::string value(1, static_cast<char>(key));
  return str.find(value) != std::string::npos;
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
