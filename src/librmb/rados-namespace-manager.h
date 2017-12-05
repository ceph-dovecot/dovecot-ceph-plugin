// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
#ifndef SRC_LIBRMB_RADOS_NAMESPACE_MANAGER_H_
#define SRC_LIBRMB_RADOS_NAMESPACE_MANAGER_H_

#include <map>
#include <string>
#include "rados-storage.h"
#include "rados-dovecot-ceph-cfg.h"
namespace librmb {

class RadosNamespaceManager {
 public:
  RadosNamespaceManager(RadosStorage *storage_, RadosDovecotCephCfg *config_) {
    this->storage = storage_;
    this->oid_suffix = "_namespace";
    this->config = config_;
  }
  virtual ~RadosNamespaceManager();
  void set_storage(RadosStorage *storage_) { this->storage = storage_; }
  void set_config(RadosDovecotCephCfg *config_) { config = config_; }
  void set_namespace_oid(std::string namespace_oid_) { this->oid_suffix = oid_suffix; }
  bool lookup_key(std::string uid, std::string *value);
  bool add_namespace_entry(std::string uid, std::string value);

 private:
  std::map<std::string, std::string> cache;
  RadosStorage *storage;
  std::string oid_suffix;
  RadosDovecotCephCfg *config;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_NAMESPACE_MANAGER_H_ */
