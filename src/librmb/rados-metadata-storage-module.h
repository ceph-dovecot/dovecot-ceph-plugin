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

#ifndef SRC_LIBRMB_RADOS_METADATA_STORAGE_MODULE_H_
#define SRC_LIBRMB_RADOS_METADATA_STORAGE_MODULE_H_

#include <rados/librados.hpp>
#include "rados-mail-object.h"

namespace librmb {
class RadosStorageMetadataModule {
 public:
  virtual ~RadosStorageMetadataModule(){};
  /* update io_ctx */
  virtual void set_io_ctx(librados::IoCtx *io_ctx){};
  /* load the metadta into RadosMailObject */
  virtual int load_metadata(RadosMailObject *mail) = 0;
  /* set a new metadata attribute to a mail object */
  virtual int set_metadata(RadosMailObject *mail, RadosMetadata &xattr) = 0;
  /* update the given metadata attributes */
  virtual bool update_metadata(const std::string &oid, std::list<RadosMetadata> &to_update) = 0;
  /* add all metadata of RadosMailObject to write_operation */
  virtual void save_metadata(librados::ObjectWriteOperation *write_op, RadosMailObject *mail) = 0;
  /* manage keywords */
  virtual int update_keyword_metadata(const std::string &oid, RadosMetadata *metadata) = 0;
  virtual int remove_keyword_metadata(const std::string &oid, std::string &key) = 0;
  virtual int load_keyword_metadata(const std::string &oid, std::set<std::string> &keys,
                                    std::map<std::string, ceph::bufferlist> *metadata) = 0;
};

}  // namespace librmb

#endif /* SRC_LIBRMB_RADOS_METADATA_STORAGE_MODULE_H_ */
