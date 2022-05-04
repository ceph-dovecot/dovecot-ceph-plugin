// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
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
#include <rados/librados.hpp>

namespace librmb {

class RadosDovecotCephCfgImpl : public RadosDovecotCephCfg {
 public:
  explicit RadosDovecotCephCfgImpl(librados::IoCtx *io_ctx_);
  RadosDovecotCephCfgImpl(RadosConfig &dovecot_cfg_, RadosCephConfig &rados_cfg_);
  virtual ~RadosDovecotCephCfgImpl() {}

  // dovecot config

  const std::string &get_rados_cluster_name() override { return dovecot_cfg.get_rbox_cluster_name(); }
  const std::string &get_rados_username() override { return dovecot_cfg.get_rados_username(); }
  void update_pool_name_metadata(const char *value) override { dovecot_cfg.update_pool_name_metadata(value); }
  const std::string &get_rados_save_log_file() override { return dovecot_cfg.get_rados_save_log_file(); }
  const std::string &get_pool_name_metadata_key() override { return dovecot_cfg.get_pool_name_metadata_key(); }
 
  int get_write_method() override { return std::stoi(dovecot_cfg.get_write_method());}


  int get_chunk_size() override { return std::stoi(dovecot_cfg.get_chunk_size());}
  std::string &get_pool_name() override { return dovecot_cfg.get_pool_name(); }

  std::string &get_key_prefix_keywords() override { return dovecot_cfg.get_key_prefix_keywords(); }
  void update_metadata(const std::string &key, const char *value_) override {
    dovecot_cfg.update_metadata(key, value_);
  }
  bool is_rbox_check_empty_mailboxes() override { return dovecot_cfg.is_rbox_check_empty_mailboxes(); }
  bool is_ceph_posix_bugfix_enabled() override { return dovecot_cfg.is_ceph_posix_bugfix_enabled(); }
  bool is_ceph_aio_wait_for_safe_and_cb() override { return dovecot_cfg.is_ceph_aio_wait_for_safe_and_cb(); }
  bool is_write_chunks() override { return dovecot_cfg.is_write_chunks(); }
  
  // rados config
  bool is_user_mapping() override { return rados_cfg.is_user_mapping(); }
  void set_config_valid(bool is_valid_) override {
    dovecot_cfg.set_config_valid(is_valid_);
    if (is_valid_) {
      rados_cfg.set_cfg_object_name(dovecot_cfg.get_rbox_cfg_object_name());
    }
  }

  void set_rbox_cfg_object_name(const std::string &value) override { dovecot_cfg.set_rbox_cfg_object_name(value); }

  std::map<std::string, std::string> *get_config() override { return dovecot_cfg.get_config(); }
  void set_io_ctx(librados::IoCtx *io_ctx_) override { rados_cfg.set_io_ctx(io_ctx_); }
  int load_rados_config() override {
    //  return dovecot_cfg.is_config_valid() ? rados_cfg.load_cfg() : -1;
    return rados_cfg.load_cfg();
  }
  int save_default_rados_config() override;
  void set_user_mapping(bool value_) override { rados_cfg.set_user_mapping(value_); }
  void set_user_ns(const std::string &ns) override { rados_cfg.set_user_ns(ns); }
  std::string &get_user_ns() override { return rados_cfg.get_user_ns(); }
  void set_user_suffix(const std::string &ns_suffix) override { rados_cfg.set_user_suffix(ns_suffix); }
  std::string &get_user_suffix() override { return rados_cfg.get_user_suffix(); }
  void set_update_attributes(const std::string &update_attributes_) override {
    rados_cfg.set_update_attributes(update_attributes_);
  }

  bool is_mail_attribute(enum rbox_metadata_key key) override { return rados_cfg.is_mail_attribute(key); }
  bool is_updateable_attribute(enum rbox_metadata_key key) override { return rados_cfg.is_updateable_attribute(key); }
  void update_mail_attributes(const char *value) override { rados_cfg.update_mail_attribute(value); }
  void update_updatable_attributes(const char *value) override { rados_cfg.update_updateable_attribute(value); }
  bool is_update_attributes() override { return rados_cfg.is_update_attributes(); }

  const std::string &get_metadata_storage_module() override { return rados_cfg.get_metadata_storage_module(); };
  const std::string &get_metadata_storage_attribute() override { return rados_cfg.get_metadata_storage_attribute(); };

  const std::string &get_mail_attributes_key() override { return rados_cfg.get_mail_attribute_key(); }
  const std::string &get_updateable_attributes_key() override { return rados_cfg.get_updateable_attribute_key(); }
  const std::string &get_update_attributes_key() override { return rados_cfg.get_update_attributes_key(); }

  bool is_config_valid() override { return dovecot_cfg.is_config_valid() && rados_cfg.is_config_valid(); }
  const std::string &get_public_namespace() override { return rados_cfg.get_public_namespace(); }
  void update_mail_attributes(const std::string &mail_attributes) {
    rados_cfg.update_mail_attribute(mail_attributes.c_str());
  }
  void update_updatable_attributes(const std::string &updateable_attributes) {
    rados_cfg.update_updateable_attribute(updateable_attributes.c_str());
  }
  int save_object(const std::string &oid, librados::bufferlist &buffer) override {
    return rados_cfg.save_object(oid, buffer);
  }
  int read_object(const std::string &oid, librados::bufferlist *buffer) override {
    return rados_cfg.read_object(oid, buffer);
  }
  void set_io_ctx_namespace(const std::string &namespace_) override { rados_cfg.set_io_ctx_namespace(namespace_); }

  RadosConfig *get_dovecot_cfg() { return &dovecot_cfg; }
  RadosCephConfig *get_rados_ceph_cfg() { return &rados_cfg; }

 private:
  RadosConfig dovecot_cfg;
  RadosCephConfig rados_cfg;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_IMPL_H_ */
