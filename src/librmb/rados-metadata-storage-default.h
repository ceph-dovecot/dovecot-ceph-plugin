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
  RadosMetadataStorageDefault(librados::IoCtx *io_ctx_);
  virtual ~RadosMetadataStorageDefault();

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

 private:
  librados::IoCtx *io_ctx;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_METADATA_STORAGE_DEFAULT_H_ */
