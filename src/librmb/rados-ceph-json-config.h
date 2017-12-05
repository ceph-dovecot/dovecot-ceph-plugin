/*
 * rados-ceph-Json-config.h
 *
 *  Created on: Nov 28, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_
#define SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_
#include <rados/librados.hpp>
#include "rados-types.h"
namespace librmb {

class RadosCephJsonConfig {
 public:
  RadosCephJsonConfig();
  virtual ~RadosCephJsonConfig() {}

  bool from_json(librados::bufferlist* buffer);
  bool to_json(librados::bufferlist* buffer);
  std::string to_string();

  const std::string& get_cfg_object_name() const { return cfg_object_name; }

  void set_cfg_object_name(const std::string& cfgObjectName) { cfg_object_name = cfgObjectName; }

  const std::string& get_generated_namespace() const { return generated_namespace; }

  void set_generated_namespace(const std::string& generatedNamespace) { generated_namespace = generatedNamespace; }

  bool is_valid() const { return valid; }

  void set_valid(bool isValid) { valid = isValid; }

  const std::string& get_ns_cfg() const { return ns_cfg; }

  void set_ns_cfg(const std::string& nsCfg) { ns_cfg = nsCfg; }

  const std::string& get_ns_suffix() const { return ns_suffix; }

  void set_ns_suffix(const std::string& nsSuffix) { ns_suffix = nsSuffix; }

  const std::string& get_public_namespace() const { return public_namespace; }

  void set_public_namespace(const std::string& public_namespace_) { public_namespace = public_namespace_; }

  void set_mail_attributes(const std::string& mail_attributes_) { mail_attributes = mail_attributes_; }
  void set_update_attributes(const std::string& update_attributes_) { update_attributes = update_attributes_; }
  void set_updateable_attributes(const std::string& updateable_attributes_) {
    updateable_attributes = updateable_attributes_;
  }

  bool is_mail_attribute(enum rbox_metadata_key key);
  bool is_updateable_attribute(enum rbox_metadata_key key);
  bool is_update_attributes() { return update_attributes.compare("true") == 0; }

  void update_mail_attribute(const char* value);
  void update_updateable_attribute(const char* value);

  const std::string& get_key_generated_namespace() const { return key_generated_namespace; }
  const std::string& get_key_ns_cfg() const { return key_ns_cfg; }
  const std::string& get_key_ns_suffix() const { return key_ns_suffix; }
  const std::string& get_key_public_namespace() const { return key_public_namespace; }

  const std::string& get_mail_attribute_key() { return key_mail_attributes; }
  const std::string& get_updateable_attribute_key() { return key_updateable_attributes; }
  const std::string& get_update_attributes_key() { return key_update_attributes; }

 private:
  bool string_contains_key(std::string& str, enum rbox_metadata_key key);
  std::string set_default_mail_attributes();
  std::string set_default_updateable_attributes();

 private:
  std::string cfg_object_name;
  bool valid;
  std::string generated_namespace;
  std::string ns_cfg;
  std::string ns_suffix;
  std::string public_namespace;

  std::string mail_attributes;
  std::string update_attributes;
  std::string updateable_attributes;

  std::string key_generated_namespace;
  std::string key_ns_cfg;
  std::string key_ns_suffix;
  std::string key_public_namespace;

  std::string key_mail_attributes;
  std::string key_update_attributes;
  std::string key_updateable_attributes;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_ */
