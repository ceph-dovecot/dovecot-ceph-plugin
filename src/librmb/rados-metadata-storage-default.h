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

#ifndef SRC_LIBRMB_RADOS_METADATA_STORAGE_DEFAULT_H_
#define SRC_LIBRMB_RADOS_METADATA_STORAGE_DEFAULT_H_
#include <list>
#include <map>
#include <string>
#include <set>
#include "rados-metadata-storage-module.h"

namespace librmb {
/**
 * Implements the default storage of mail metadata.
 *
 * Each metadata attribute is saved as single xattribute.
 *
 */
class RadosMetadataStorageDefault : public RadosStorageMetadataModule {
 public:
  explicit RadosMetadataStorageDefault(librados::IoCtx *io_ctx_);
  virtual ~RadosMetadataStorageDefault();
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

 private:
  librados::IoCtx *io_ctx;
};

} /* namespace librmb */

#endif  // SRC_LIBRMB_RADOS_METADATA_STORAGE_DEFAULT_H_
