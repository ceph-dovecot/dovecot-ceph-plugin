/*
 * rados-config.cpp
 *
 *  Created on: Nov 1, 2017
 *      Author: jan
 */

#include "rados-dovecot-config.h"

namespace librmb {

RadosConfig::RadosConfig() {
  // set defaults
  rbox_cluster_name = "rbox_cluster_name";
  rados_username = "rados_user_name";

  pool_name = "rbox_pool_name";

  mail_attributes = "rbox_mail_attributes";
  updateable_attributes = "rbox_updateable_attributes";
  update_attributes = "rbox_update_attributes";
  rbox_cfg_object_name = "rbox_cfg_object_name";

  config[pool_name] = "mail_storage";
  config[update_attributes] = "false";
  config[mail_attributes] = set_default_mail_attributes();
  config[updateable_attributes] = set_default_updateable_attributes();

  config[rbox_cfg_object_name] = "rbox_cfg";
  config[rbox_cluster_name] = "ceph";
  config[rados_username] = "client.admin";
  is_valid = false;
}

std::string RadosConfig::set_default_mail_attributes() {
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

std::string RadosConfig::set_default_updateable_attributes() {
  std::string mutable_attributes;
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_MAILBOX_GUID)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_ORIG_MAILBOX)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_OLDV1_SAVE_TIME)));
  return mutable_attributes;
}

bool RadosConfig::is_mail_attribute(enum rbox_metadata_key key) {
  return string_contains_key(config[mail_attributes], key);
}

bool RadosConfig::is_updateable_attribute(enum rbox_metadata_key key) {
  return string_contains_key(config[updateable_attributes], key);
}

bool RadosConfig::string_contains_key(std::string &str, enum rbox_metadata_key key) {
  std::string value(1, static_cast<char>(key));
  return str.find(value) != std::string::npos;
}

void RadosConfig::update_mail_attribute(const char *value) {
  if (value == NULL) {
    return;
  }
  config[mail_attributes] = value;
}
void RadosConfig::update_updateable_attribute(const char *value) {
  if (value == NULL) {
    return;
  }
  config[updateable_attributes] = value;
}

void RadosConfig::update_pool_name_metadata(const char *value) {
  if (value == NULL) {
    return;
  }
  config[pool_name] = value;
}
void RadosConfig::update_metadata(std::string &key, const char *value_) {
  if (value_ == NULL) {
    return;
  }
  if (config.find(key) != config.end()) {
    std::string value = value_;
    config[key] = value;
  }
}

RadosConfig::~RadosConfig() {
  // TODO Auto-generated destructor stub
}

} /* namespace librmb */
