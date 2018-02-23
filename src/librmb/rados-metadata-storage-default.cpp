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

#include "rados-metadata-storage-default.h"
#include "rados-util.h"

namespace librmb {

std::string RadosMetadataStorageDefault::module_name = "default";

RadosMetadataStorageDefault::RadosMetadataStorageDefault(librados::IoCtx *io_ctx_) { this->io_ctx = io_ctx_; }

RadosMetadataStorageDefault::~RadosMetadataStorageDefault() {}


int RadosMetadataStorageDefault::load_metadata(RadosMailObject *mail) {
  int ret = -1;
  if (mail != nullptr) {
    if (mail->get_metadata()->size() == 0) {
      ret = io_ctx->getxattrs(mail->get_oid(), *mail->get_metadata());
    } else {
      ret = 0;
    }
    if (ret >= 0) {
      ret = RadosUtils::get_all_keys_and_values(io_ctx, mail->get_oid(), mail->get_extended_metadata());
    }
  }
  return ret;
}
int RadosMetadataStorageDefault::set_metadata(RadosMailObject *mail, RadosMetadata &xattr) {
  mail->add_metadata(xattr);
  return io_ctx->setxattr(mail->get_oid(), xattr.key.c_str(), xattr.bl);
}

void RadosMetadataStorageDefault::save_metadata(librados::ObjectWriteOperation *write_op, RadosMailObject *mail) {
  // update metadata
  for (std::map<string, ceph::bufferlist>::iterator it = mail->get_metadata()->begin();
       it != mail->get_metadata()->end(); ++it) {
    write_op->setxattr((*it).first.c_str(), (*it).second);
  }
  if (mail->get_extended_metadata()->size() > 0) {
    write_op->omap_set(*mail->get_extended_metadata());
  }
}
bool RadosMetadataStorageDefault::update_metadata(std::string &oid, std::list<RadosMetadata> &to_update) {
  librados::ObjectWriteOperation write_op;
  librados::AioCompletion *completion = librados::Rados::aio_create_completion();

  // update metadata
  for (std::list<RadosMetadata>::iterator it = to_update.begin(); it != to_update.end(); ++it) {
    write_op.setxattr((*it).key.c_str(), (*it).bl);
  }

  int ret = io_ctx->aio_operate(oid, completion, &write_op);
  completion->wait_for_complete();
  completion->release();
  return ret == 0;
}
int RadosMetadataStorageDefault::update_keyword_metadata(std::string &oid, RadosMetadata *metadata) {
  int ret = -1;
  if (metadata != nullptr) {
    std::map<std::string, librados::bufferlist> map;
    map.insert(std::pair<string, librados::bufferlist>(metadata->key, metadata->bl));
    ret = io_ctx->omap_set(oid, map);
  }
  return ret;
}
int RadosMetadataStorageDefault::remove_keyword_metadata(std::string &oid, std::string &key) {
  std::set<std::string> keys;
  keys.insert(key);
  return io_ctx->omap_rm_keys(oid, keys);
}
int RadosMetadataStorageDefault::load_keyword_metadata(std::string &oid, std::set<std::string> &keys,
                                                        std::map<std::string, ceph::bufferlist> *metadata) {
  return io_ctx->omap_get_vals_by_keys(oid, keys, metadata);
}

} /* namespace librmb */
