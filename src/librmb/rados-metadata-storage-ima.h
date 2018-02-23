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

class RadosMetadataStorageIma : public RadosStorageMetadataModule {
 private:
  int parse_attribute(RadosMailObject *mail, json_t *root);
  int get_all_keys_and_values(const std::string &oid, std::map<std::string, librados::bufferlist> *kv_map);

 public:
  RadosMetadataStorageIma(librados::IoCtx *io_ctx_, RadosDovecotCephCfg *cfg_);
  virtual ~RadosMetadataStorageIma();

  int load_metadata(RadosMailObject *mail);
  int set_metadata(RadosMailObject *mail, RadosMetadata &xattr);
  bool update_metadata(std::string &oid, std::list<RadosMetadata> &to_update);
  void save_metadata(librados::ObjectWriteOperation *write_op, RadosMailObject *mail);

  int update_keyword_metadata(std::string &oid, RadosMetadata *metadata);
  int remove_keyword_metadata(std::string &oid, std::string &key);
  int load_keyword_metadata(std::string &oid, std::set<std::string> &keys,
                             std::map<std::string, ceph::bufferlist> *metadata);

 public:
  static std::string module_name;
  static std::string keyword_key;

 private:
  librados::IoCtx *io_ctx;
  RadosDovecotCephCfg *cfg;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_METADATA_STORAGE_IMA_H_ */
