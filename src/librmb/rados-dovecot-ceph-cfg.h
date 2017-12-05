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
#ifndef SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_H_
#define SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_H_

#include "rados-types.h"
#include <string>
#include <map>
#include "rados-storage.h"
namespace librmb {

class RadosDovecotCephCfg {
 public:
  virtual ~RadosDovecotCephCfg(){};

  // dovecot configuration
  virtual const std::string &get_rados_cluster_name() = 0;
  virtual const std::string &get_rados_username() = 0;
  virtual bool is_mutable_metadata(enum rbox_metadata_key key) = 0;
  virtual bool is_immutable_metadata(enum rbox_metadata_key key) = 0;
  virtual void update_mutable_metadata(const char *value) = 0;
  virtual void update_immutable_metadata(const char *value) = 0;
  virtual void update_pool_name_metadata(const char *value) = 0;

  virtual const std::string &get_mutable_metadata_key() = 0;
  virtual const std::string &get_immutable_metadata_key() = 0;
  virtual const std::string &get_pool_name_metadata_key() = 0;
  virtual const std::string &get_update_immutable_key() = 0;
  virtual std::map<std::string, std::string> *get_config() = 0;

  virtual std::string get_pool_name() = 0;
  virtual bool is_update_immutable() = 0;


  virtual void update_metadata(std::string &key, const char *value_) = 0;
  virtual bool is_config_valid() = 0;
  virtual void set_config_valid(bool is_valid_) = 0;

  virtual std::string get_key_prefix_keywords() = 0;

  // ceph configuration
  virtual void set_storage(RadosStorage *storage) = 0;
  virtual int load_rados_config() = 0;
  virtual int save_default_rados_config() = 0;
  virtual void set_generated_namespace(bool value_) = 0;
  virtual bool is_generated_namespace() = 0;
  virtual void set_ns_cfg(std::string &ns) = 0;
  virtual std::string get_ns_cfg() = 0;
  virtual void set_ns_suffix(std::string &ns_suffix) = 0;
  virtual std::string get_ns_suffix() = 0;
  virtual const std::string &get_public_namespace() = 0;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_H_ */
