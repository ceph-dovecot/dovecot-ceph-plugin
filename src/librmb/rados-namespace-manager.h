/*
 * rados-namespace-manager.h
 *
 *  Created on: Nov 17, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_RADOS_NAMESPACE_MANAGER_H_
#define SRC_LIBRMB_RADOS_NAMESPACE_MANAGER_H_

#include <map>
#include <string>
#include "rados-storage.h"

namespace librmb {

class RadosNamespaceManager {
 public:
  RadosNamespaceManager(RadosStorage *storage_) {
    this->storage = storage_;
    this->oid_suffix = "_namespace";
  }
  virtual ~RadosNamespaceManager();
  void set_storage(RadosStorage *storage_) { this->storage = storage_; }
  void set_namespace_oid(std::string namespace_oid_) { this->oid_suffix = oid_suffix; }
  bool lookup_key(std::string uid, std::string *value);
  bool add_namespace_entry(std::string uid, std::string value);

 private:
  std::map<std::string, std::string> cache;
  RadosStorage *storage;
  std::string oid_suffix;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_NAMESPACE_MANAGER_H_ */
