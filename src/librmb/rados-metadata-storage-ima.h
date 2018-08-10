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

#ifndef SRC_LIBRMB_RADOS_METADATA_STORAGE_IMA_H_
#define SRC_LIBRMB_RADOS_METADATA_STORAGE_IMA_H_

#include "rados-ceph-config.h"
#include "rados-dovecot-ceph-cfg.h"
#include "rados-metadata-storage-module.h"
#include <jansson.h>

namespace librmb {
/*
 *  All immutable mail attributes are saved in one rados
 *  attribute. The value of the attribute is a json format
 *
 * If a attribute changes from immutable to mutable, a
 * new attribute is added to the mail object, which overrides the
 * immutable value.
 *
 */
class RadosMetadataStorageIma : public RadosStorageMetadataModule {
 private:
  int parse_attribute(RadosMail *mail, json_t *root);

 public:
  RadosMetadataStorageIma(librados::IoCtx *io_ctx_, RadosDovecotCephCfg *cfg_);
  virtual ~RadosMetadataStorageIma();
  void set_io_ctx(librados::IoCtx *io_ctx_) override { this->io_ctx = io_ctx_; }
  int load_metadata(RadosMail *mail) override;
  int set_metadata(RadosMail *mail, RadosMetadata &xattr) override;
  bool update_metadata(const std::string &oid, std::list<RadosMetadata> &to_update) override;
  void save_metadata(librados::ObjectWriteOperation *write_op, RadosMail *mail) override;

  int update_keyword_metadata(const std::string &oid, RadosMetadata *metadata) override;
  int remove_keyword_metadata(const std::string &oid, std::string &key) override;
  int load_keyword_metadata(const std::string &oid, std::set<std::string> &keys,
                            std::map<std::string, ceph::bufferlist> *metadata) override;

 public:
  static std::string module_name;
  static std::string keyword_key;

 private:
  librados::IoCtx *io_ctx;
  RadosDovecotCephCfg *cfg;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_METADATA_STORAGE_IMA_H_ */
