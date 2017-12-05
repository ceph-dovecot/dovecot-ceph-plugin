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
  mutable_metadata = "rbox_mutable_metadata";
  immutable_metadata = "rbox_immutable_metadata";
  update_immutable = "rbox_update_immutable";
  generate_namespace = "rbox_generate_namespace";
  rbox_cfg_object_name = "rbox_cfg_object_name";

  config[pool_name] = "mail_storage";
  config[update_immutable] = "false";
  config[mutable_metadata] = set_default_mutable_attributes();
  config[immutable_metadata] = set_default_immutable_attributes();
  config[generate_namespace] = "false";
  config[rbox_cfg_object_name] = "rbox_cfg";
  config[rbox_cluster_name] = "ceph";
  config[rados_username] = "client.admin";
  is_valid = false;
}

std::string RadosConfig::set_default_mutable_attributes() {
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

std::string RadosConfig::set_default_immutable_attributes() {
  std::string mutable_attributes;
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_MAILBOX_GUID)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_ORIG_MAILBOX)));
  mutable_attributes.append(std::string(1, static_cast<char>(RBOX_METADATA_OLDV1_SAVE_TIME)));
  return mutable_attributes;
}

bool RadosConfig::is_mutable_metadata(enum rbox_metadata_key key) {
  return string_contains_key(config[mutable_metadata], key);
}

bool RadosConfig::is_immutable_metadata(enum rbox_metadata_key key) {
  return string_contains_key(config[immutable_metadata], key);
}

bool RadosConfig::string_contains_key(std::string &str, enum rbox_metadata_key key) {
  std::string value(1, static_cast<char>(key));
  return str.find(value) != std::string::npos;
}

void RadosConfig::update_mutable_metadata(const char *value) {
  if (value == NULL) {
    return;
  }
  config[mutable_metadata] = value;
}
void RadosConfig::update_immutable_metadata(const char *value) {
  if (value == NULL) {
    return;
  }
  config[immutable_metadata] = value;
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
