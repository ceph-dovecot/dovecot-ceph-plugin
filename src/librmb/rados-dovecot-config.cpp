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

  rbox_cfg_object_name = "rbox_cfg_object_name";

  config[pool_name] = "mail_storage";

  config[rbox_cfg_object_name] = "rbox_cfg";
  config[rbox_cluster_name] = "ceph";
  config[rados_username] = "client.admin";
  is_valid = false;
}


bool RadosConfig::string_contains_key(std::string &str, enum rbox_metadata_key key) {
  std::string value(1, static_cast<char>(key));
  return str.find(value) != std::string::npos;
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
