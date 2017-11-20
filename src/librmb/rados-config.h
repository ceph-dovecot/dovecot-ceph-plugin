/*
 * rados-config.h
 *
 *  Created on: Nov 1, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_RADOS_CONFIG_H_
#define SRC_LIBRMB_RADOS_CONFIG_H_

#include <map>
#include "rados-types.h"

namespace librmb {

class RadosConfig {
 public:
  RadosConfig();
  virtual ~RadosConfig();
  std::string set_default_mutable_attributes();
  std::string set_default_immutable_attributes();

  bool is_mutable_metadata(enum rbox_metadata_key key);
  bool is_immutable_metadata(enum rbox_metadata_key key);
  void update_mutable_metadata(const char *value);
  void update_immutable_metadata(const char *value);
  void update_pool_name_metadata(const char *value);

  const std::string &get_mutable_metadata_key() { return mutable_metadata; }
  const std::string &get_immutable_metadata_key() { return immutable_metadata; }
  const std::string &get_pool_name_metadata_key() { return pool_name; }
  const std::string &get_update_immutable_key() { return update_immutable; }
  std::map<std::string, std::string> *get_config() { return &config; }

  std::string get_pool_name() { return config[pool_name]; }
  bool is_update_immutable() { return config[update_immutable].compare("true") == 0; }

  bool is_generated_namespace() { return config[generate_namespace].compare("true") == 0; }

  void update_metadata(std::string &key, const char *value_);
  bool is_config_valid() { return is_valid; }
  void set_config_valid(bool is_valid_) { this->is_valid = is_valid_; }

  std::string get_key_prefix_keywords() { return "k"; }
  void set_generated_namespace(bool value_) { config[generate_namespace] = value_ ? "true" : "false"; }

 private:
  bool string_contains_key(std::string &str, enum rbox_metadata_key key);

 private:
  std::map<std::string, std::string> config;
  std::string mutable_metadata;
  std::string pool_name;
  std::string update_immutable;
  std::string immutable_metadata;
  std::string generate_namespace;
  bool is_valid;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_CONFIG_H_ */
