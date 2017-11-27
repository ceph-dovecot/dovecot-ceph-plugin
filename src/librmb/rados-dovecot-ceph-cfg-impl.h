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

#ifndef SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_IMPL_H_
#define SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_IMPL_H_

#include "rados-dovecot-ceph-cfg.h"
#include "rados-ceph-config.h"

#include <string>
#include <map>
#include "rados-dovecot-config.h"

namespace librmb {

class RadosDovecotCephCfgImpl : public RadosDovecotCephCfg {
 public:
  RadosDovecotCephCfgImpl(RadosStorage *storage);
  virtual ~RadosDovecotCephCfgImpl();

  // dovecot config
  bool is_mutable_metadata(enum rbox_metadata_key key) { return dovecot_cfg->is_mutable_metadata(key); }
  bool is_immutable_metadata(enum rbox_metadata_key key) { return dovecot_cfg->is_immutable_metadata(key); }
  void update_mutable_metadata(const char *value) { dovecot_cfg->update_mutable_metadata(value); }
  void update_immutable_metadata(const char *value) { dovecot_cfg->update_immutable_metadata(value); }
  void update_pool_name_metadata(const char *value) { dovecot_cfg->update_pool_name_metadata(value); }

  const std::string &get_mutable_metadata_key() { return dovecot_cfg->get_mutable_metadata_key(); }
  const std::string &get_immutable_metadata_key() { return dovecot_cfg->get_immutable_metadata_key(); }
  const std::string &get_pool_name_metadata_key() { return dovecot_cfg->get_pool_name_metadata_key(); }
  const std::string &get_update_immutable_key() { return dovecot_cfg->get_update_immutable_key(); }

  std::string get_pool_name() { return dovecot_cfg->get_pool_name(); }
  bool is_update_immutable() { return dovecot_cfg->is_update_immutable(); }
  void update_metadata(std::string &key, const char *value_) { dovecot_cfg->update_metadata(key, value_); }
  std::string get_key_prefix_keywords() { return dovecot_cfg->get_key_prefix_keywords(); }

  // rados config
  bool is_generated_namespace() { return rados_cfg->is_generated_namespace(); }
  void set_config_valid(bool is_valid_) {
    dovecot_cfg->set_config_valid(is_valid_);
    if (is_valid_) {
      rados_cfg->set_cfg_object_name(dovecot_cfg->get_rbox_cfg_object_name());
    }
  }

  std::map<std::string, std::string> *get_config() { return dovecot_cfg->get_config(); }
  void set_storage(RadosStorage *storage) { rados_cfg->set_storage(storage); }
  int load_rados_config() {
    return dovecot_cfg->is_config_valid() ? rados_cfg->load_cfg() : -1;
  }

  void set_generated_namespace(bool value_) { rados_cfg->set_generated_namespace(value_); }
  void set_ns_cfg(std::string &ns) { rados_cfg->set_ns_cfg(ns); }
  std::string get_ns_cfg() { return rados_cfg->get_ns_cfg(); }
  void set_ns_suffix(std::string &ns_suffix) { rados_cfg->set_ns_suffix(ns_suffix); }
  std::string get_ns_suffix() { return rados_cfg->get_ns_suffix(); }

  bool is_config_valid() { return dovecot_cfg->is_config_valid() && rados_cfg->is_config_valid(); }

 private:
  RadosConfig *dovecot_cfg;
  RadosCephConfig *rados_cfg;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_IMPL_H_ */
