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
  std::string set_default_mail_attributes();
  std::string set_default_updateable_attributes();

  bool is_mail_attribute(enum rbox_metadata_key key);
  bool is_updateable_attribute(enum rbox_metadata_key key);
  void update_mail_attribute(const char *value);
  void update_updateable_attribute(const char *value);
  void update_pool_name_metadata(const char *value);

  const std::string &get_mail_attribute_key() { return mail_attributes; }
  const std::string &get_updateable_attribute_key() { return updateable_attributes; }
  const std::string &get_pool_name_metadata_key() { return pool_name; }
  const std::string &get_update_attributes_key() { return update_attributes; }
  std::map<std::string, std::string> *get_config() { return &config; }

  std::string get_pool_name() { return config[pool_name]; }
  bool is_update_attributes() { return config[update_attributes].compare("true") == 0; }

  void update_metadata(std::string &key, const char *value_);
  bool is_config_valid() { return is_valid; }
  void set_config_valid(bool is_valid_) { this->is_valid = is_valid_; }

  std::string get_key_prefix_keywords() { return "k"; }

  std::string get_rbox_cfg_object_name() { return config[rbox_cfg_object_name]; }

  const std::string &get_rbox_cluster_name() { return config[rbox_cluster_name]; }
  const std::string &get_rados_username() { return config[rados_username]; }

 private:
  bool string_contains_key(std::string &str, enum rbox_metadata_key key);

 private:
  std::map<std::string, std::string> config;
  std::string mail_attributes;
  std::string pool_name;
  std::string update_attributes;
  std::string updateable_attributes;
  std::string rbox_cfg_object_name;
  std::string rbox_cluster_name;
  std::string rados_username;
  bool is_valid;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_DOVECOT_CONFIG_H_ */
