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
  RadosDovecotCephCfgImpl(RadosConfig *dovecot_cfg_, RadosCephConfig *rados_cfg_);
  virtual ~RadosDovecotCephCfgImpl();

  // dovecot config

  const std::string &get_rados_cluster_name() { return dovecot_cfg->get_rbox_cluster_name(); }
  const std::string &get_rados_username() { return dovecot_cfg->get_rados_username(); }
  void update_pool_name_metadata(const char *value) { dovecot_cfg->update_pool_name_metadata(value); }

  const std::string &get_pool_name_metadata_key() { return dovecot_cfg->get_pool_name_metadata_key(); }

  std::string get_pool_name() { return dovecot_cfg->get_pool_name(); }

  std::string get_key_prefix_keywords() { return dovecot_cfg->get_key_prefix_keywords(); }
  void update_metadata(std::string &key, const char *value_) { dovecot_cfg->update_metadata(key, value_); }

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
  int save_default_rados_config();
  void set_generated_namespace(bool value_) { rados_cfg->set_generated_namespace(value_); }
  void set_ns_cfg(std::string &ns) { rados_cfg->set_ns_cfg(ns); }
  std::string get_ns_cfg() { return rados_cfg->get_ns_cfg(); }
  void set_ns_suffix(std::string &ns_suffix) { rados_cfg->set_ns_suffix(ns_suffix); }
  std::string get_ns_suffix() { return rados_cfg->get_ns_suffix(); }

  bool is_mail_attribute(enum rbox_metadata_key key) { return rados_cfg->is_mail_attribute(key); }
  bool is_updateable_attribute(enum rbox_metadata_key key) { return rados_cfg->is_updateable_attribute(key); }
  void update_mail_attributes(const char *value) { rados_cfg->update_mail_attribute(value); }
  void update_updatable_attributes(const char *value) { rados_cfg->update_updateable_attribute(value); }
  bool is_update_attributes() { return rados_cfg->is_update_attributes(); }

  const std::string &get_mail_attributes_key() { return rados_cfg->get_mail_attribute_key(); }
  const std::string &get_updateable_attributes_key() { return rados_cfg->get_updateable_attribute_key(); }
  const std::string &get_update_attributes_key() { return rados_cfg->get_update_attributes_key(); }

  bool is_config_valid() { return dovecot_cfg->is_config_valid() && rados_cfg->is_config_valid(); }
  const std::string &get_public_namespace() { return rados_cfg->get_public_namespace(); }
  void update_mail_attributes(const std::string &mail_attributes) {
    rados_cfg->update_mail_attribute(mail_attributes.c_str());
  }
  void update_updatable_attributes(const std::string &updateable_attributes) {
    rados_cfg->update_updateable_attribute(updateable_attributes.c_str());
  }

 private:
  RadosConfig *dovecot_cfg;
  RadosCephConfig *rados_cfg;
  bool delete_cfg;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_IMPL_H_ */
