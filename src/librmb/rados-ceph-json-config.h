/*
 * rados-ceph-Json-config.h
 *
 *  Created on: Nov 28, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_
#define SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_
#include <rados/librados.hpp>
namespace librmb {

class RadosCephJsonConfig {
 public:
  RadosCephJsonConfig();
  virtual ~RadosCephJsonConfig() {}

  bool from_json(librados::bufferlist* buffer);
  bool to_json(librados::bufferlist* buffer);

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

 private:
  std::string cfg_object_name;
  bool valid;
  std::string generated_namespace;
  std::string ns_cfg;
  std::string ns_suffix;

  std::string key_generated_namespace;
  std::string key_ns_cfg;
  std::string key_ns_suffix;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_CEPH_JSON_CONFIG_H_ */
