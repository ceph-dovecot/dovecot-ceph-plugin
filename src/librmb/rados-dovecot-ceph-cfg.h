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
#ifndef SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_H_
#define SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_H_

#include "rados-types.h"
#include <string>
#include <map>
#include "rados-storage.h"
namespace librmb {

/**
 * class RadosDovecotCephCfg
 *
 * Abstract interface to the Dovecot-Ceph-plugin configuration.
 * See project wiki for more details.
 *
 */
class RadosDovecotCephCfg {
 public:
  virtual ~RadosDovecotCephCfg(){};

  // dovecot configuration
  virtual const std::string &get_rados_cluster_name() = 0;
  virtual const std::string &get_rados_username() = 0;
  virtual const std::string &get_rados_save_log_file() = 0;
  virtual bool is_mail_attribute(enum rbox_metadata_key key) = 0;
  virtual bool is_updateable_attribute(enum rbox_metadata_key key) = 0;
  virtual void set_update_attributes(const std::string &update_attributes_) = 0;
  virtual void update_mail_attributes(const char *value) = 0;
  virtual void update_updatable_attributes(const char *value) = 0;
  virtual void update_pool_name_metadata(const char *value) = 0;
  virtual bool is_ceph_posix_bugfix_enabled() = 0;
  virtual bool is_ceph_aio_wait_for_safe_and_cb() = 0;
  virtual bool is_write_chunks() = 0;
  virtual int get_chunk_size() = 0;
  virtual int get_write_method() = 0;

  virtual int get_object_search_method()  = 0;
  virtual int get_object_search_threads() = 0;

  virtual const std::string &get_pool_name_metadata_key() = 0;
  virtual const std::string &get_update_attributes_key() = 0;
  virtual const std::string &get_mail_attributes_key() = 0;
  virtual const std::string &get_updateable_attributes_key() = 0;

  virtual const std::string &get_metadata_storage_module() = 0;
  virtual const std::string &get_metadata_storage_attribute() = 0;

  virtual std::map<std::string, std::string> *get_config() = 0;

  virtual std::string &get_pool_name() = 0;
  virtual std::string &get_index_pool_name() = 0;
  
  virtual bool is_update_attributes() = 0;

  virtual void set_rbox_cfg_object_name(const std::string &value) = 0;
  virtual void update_metadata(const std::string &key, const char *value_) = 0;
  virtual bool is_config_valid() = 0;
  virtual void set_config_valid(bool is_valid_) = 0;

  virtual std::string &get_key_prefix_keywords() = 0;

  virtual void set_io_ctx(librados::IoCtx *io_ctx) = 0;
  virtual int load_rados_config() = 0;
  virtual int save_default_rados_config() = 0;
  virtual void set_user_mapping(bool value_) = 0;
  virtual bool is_user_mapping() = 0;
  virtual void set_user_ns(const std::string &ns) = 0;
  virtual std::string &get_user_ns() = 0;
  virtual void set_user_suffix(const std::string &ns_suffix) = 0;
  virtual std::string &get_user_suffix() = 0;
  virtual const std::string &get_public_namespace() = 0;

  /*!
   * Save configuration to objectt
   * @param[in] unique ident
   * @param[in] buffer configuration object
   * * @return linux error codes or 0 if successful
   */
  virtual int save_object(const std::string &oid, librados::bufferlist &buffer) = 0;
  /*!
   * read configuration from object
   * @param[in] unique ident
   * @param[out] valid pointer to buffer.
   * @return linux error codes or 0 if successful
   */
  virtual int read_object(const std::string &oid, librados::bufferlist *buffer) = 0;
  /*!
   * set rados configuration namespace
   */
  virtual void set_io_ctx_namespace(const std::string &namespace_) = 0;

  virtual bool is_rbox_check_empty_mailboxes() = 0;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_DOVECOT_CEPH_CFG_H_ */
