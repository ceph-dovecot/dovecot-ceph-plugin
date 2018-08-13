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

#include "rados-metadata-storage-ima.h"
#include "rados-util.h"
#include <string.h>
#include <utility>

std::string librmb::RadosMetadataStorageIma::module_name = "ima";
std::string librmb::RadosMetadataStorageIma::keyword_key = "K";
namespace librmb {

RadosMetadataStorageIma::RadosMetadataStorageIma(librados::IoCtx *io_ctx_, RadosDovecotCephCfg *cfg_) {
  this->io_ctx = io_ctx_;
  this->cfg = cfg_;
}

RadosMetadataStorageIma::~RadosMetadataStorageIma() {}

int RadosMetadataStorageIma::parse_attribute(RadosMail *mail, json_t *root) {
  std::string key;
  void *iter = json_object_iter(root);

  while (iter) {
    key = json_object_iter_key(iter);
    json_t *value = json_object_iter_value(iter);

    if (key.compare(RadosMetadataStorageIma::keyword_key) == 0) {
      std::string _keyword_key;
      void *keyword_iter = json_object_iter(value);
      while (keyword_iter) {
        librados::bufferlist bl;
        _keyword_key = json_object_iter_key(keyword_iter);
        json_t *keyword_value = json_object_iter_value(keyword_iter);
        bl.append(json_string_value(keyword_value));

        (*mail->get_extended_metadata())[_keyword_key] = bl;

        keyword_iter = json_object_iter_next(value, keyword_iter);
      }
    } else {
      librados::bufferlist bl;
      bl.append(json_string_value(value));
      (*mail->get_metadata())[key] = bl;
    }
    iter = json_object_iter_next(root, iter);
  }
  return 0;
}

int RadosMetadataStorageIma::load_metadata(RadosMail *mail) {
  if (mail == nullptr) {
    return -1;
  }
  if (mail->get_metadata()->size() > 0) {
    return 0;
  }

  std::map<string, ceph::bufferlist> attr;
  int ret = io_ctx->getxattrs(mail->get_oid(), attr);
  if (ret < 0) {
    return ret;
  }

  if (attr.find(cfg->get_metadata_storage_attribute()) != attr.end()) {
    // json object for immutable attributes.
    json_t *root;
    json_error_t error;
    root = json_loads(attr[cfg->get_metadata_storage_attribute()].to_str().c_str(), 0, &error);
    parse_attribute(mail, root);

    json_decref(root);
  }

  // load other attributes
  for (std::map<string, ceph::bufferlist>::iterator it = attr.begin(); it != attr.end(); ++it) {
    if ((*it).first.compare(cfg->get_metadata_storage_attribute()) != 0) {
      (*mail->get_metadata())[(*it).first] = (*it).second;
    }
  }

  // load other omap values.
  if (cfg->is_updateable_attribute(librmb::RBOX_METADATA_OLDV1_KEYWORDS)) {
    ret = RadosUtils::get_all_keys_and_values(io_ctx, mail->get_oid(), mail->get_extended_metadata());
  }

  return ret;
}

// it is required that mail->get_metadata is up to date before update.
int RadosMetadataStorageIma::set_metadata(RadosMail *mail, RadosMetadata &xattr) {
  enum rbox_metadata_key k = static_cast<enum rbox_metadata_key>(*xattr.key.c_str());
  if (!cfg->is_updateable_attribute(k)) {
    mail->add_metadata(xattr);
    librados::ObjectWriteOperation op;
    save_metadata(&op, mail);
    return io_ctx->operate(mail->get_oid(), &op);
  } else {
    return io_ctx->setxattr(mail->get_oid(), xattr.key.c_str(), xattr.bl);
  }
}

void RadosMetadataStorageIma::save_metadata(librados::ObjectWriteOperation *write_op, RadosMail *mail) {
  char *s = NULL;
  json_t *root = json_object();
  librados::bufferlist bl;
  if (mail->get_metadata()->size() > 0) {
    for (std::map<string, ceph::bufferlist>::iterator it = mail->get_metadata()->begin();
         it != mail->get_metadata()->end(); ++it) {
      enum rbox_metadata_key k = static_cast<enum rbox_metadata_key>(*(*it).first.c_str());
      if (!cfg->is_updateable_attribute(k) || !cfg->is_update_attributes()) {
        json_object_set_new(root, (*it).first.c_str(), json_string((*it).second.to_str().c_str()));
      } else {
        write_op->setxattr((*it).first.c_str(), (*it).second);
      }
    }
  }
  json_t *keyword = json_object();
  // build extended Metadata object
  if (mail->get_extended_metadata()->size() > 0) {
    if (!cfg->is_updateable_attribute(librmb::RBOX_METADATA_OLDV1_KEYWORDS) || !cfg->is_update_attributes()) {
      for (std::map<string, ceph::bufferlist>::iterator it = mail->get_extended_metadata()->begin();
           it != mail->get_extended_metadata()->end(); ++it) {
        json_object_set_new(keyword, (*it).first.c_str(), json_string((*it).second.to_str().c_str()));
      }
      json_object_set_new(root, RadosMetadataStorageIma::keyword_key.c_str(), keyword);
    } else {
      write_op->omap_set(*mail->get_extended_metadata());
    }
  }

  s = json_dumps(root, 0);
  bl.append(s);
  free(s);
  json_decref(keyword);
  json_decref(root);

  write_op->setxattr(cfg->get_metadata_storage_attribute().c_str(), bl);
}

bool RadosMetadataStorageIma::update_metadata(const std::string &oid, std::list<RadosMetadata> &to_update) {
  librados::ObjectWriteOperation write_op;

  if (to_update.empty()) {
    return true;
  }

  RadosMail obj;
  obj.set_oid(oid);
  load_metadata(&obj);

  // update metadata
  for (std::list<RadosMetadata>::iterator it = to_update.begin(); it != to_update.end(); ++it) {
    (*obj.get_extended_metadata())[(*it).key] = (*it).bl;
  }

  // write update
  save_metadata(&write_op, &obj);
  librados::AioCompletion *completion = librados::Rados::aio_create_completion();
  int ret = io_ctx->aio_operate(oid, completion, &write_op);
  completion->wait_for_complete();
  completion->release();
  return ret == 0;
}
int RadosMetadataStorageIma::update_keyword_metadata(const std::string &oid, RadosMetadata *metadata) {
  int ret = -1;
  if (metadata != nullptr) {
    if (!cfg->is_updateable_attribute(librmb::RBOX_METADATA_OLDV1_KEYWORDS) || !cfg->is_update_attributes()) {
    } else {
      std::map<std::string, librados::bufferlist> map;
      map.insert(std::pair<string, librados::bufferlist>(metadata->key, metadata->bl));
      ret = io_ctx->omap_set(oid, map);
    }
  }
  return ret;
}
int RadosMetadataStorageIma::remove_keyword_metadata(const std::string &oid, std::string &key) {
  std::set<std::string> keys;
  keys.insert(key);
  return io_ctx->omap_rm_keys(oid, keys);
}

int RadosMetadataStorageIma::load_keyword_metadata(const std::string &oid, std::set<std::string> &keys,
                                                   std::map<std::string, ceph::bufferlist> *metadata) {
  return io_ctx->omap_get_vals_by_keys(oid, keys, metadata);
}

} /* namespace librmb */
