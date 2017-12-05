/*
 * rados-config.h
 *
 *  Created on: Nov 1, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_
#define SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_

#include <map>
#include "rados-types.h"

namespace librmb {

class RadosConfig {
 public:
  RadosConfig();
  virtual ~RadosConfig();

  void update_pool_name_metadata(const char *value);

  const std::string &get_pool_name_metadata_key() { return pool_name; }

  std::map<std::string, std::string> *get_config() { return &config; }

  std::string get_pool_name() { return config[pool_name]; }

  bool is_config_valid() { return is_valid; }
  void set_config_valid(bool is_valid_) { this->is_valid = is_valid_; }

  std::string get_key_prefix_keywords() { return "k"; }

  std::string get_rbox_cfg_object_name() { return config[rbox_cfg_object_name]; }

  const std::string &get_rbox_cluster_name() { return config[rbox_cluster_name]; }
  const std::string &get_rados_username() { return config[rados_username]; }
  void update_metadata(std::string &key, const char *value_);

 private:
  bool string_contains_key(std::string &str, enum rbox_metadata_key key);

 private:
  std::map<std::string, std::string> config;
  std::string pool_name;

  std::string rbox_cfg_object_name;
  std::string rbox_cluster_name;
  std::string rados_username;
  bool is_valid;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_ */
