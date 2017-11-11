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

#ifndef SRC_LIBRMB_RADOS_METADATA_H_
#define SRC_LIBRMB_RADOS_METADATA_H_

#include "rados-types.h"
#include <string>
#include "time.h"
#include <stdlib.h>
#include <rados/librados.hpp>

namespace librmb {

class RadosMetadata {
 public:
  RadosMetadata(std::string& key_, std::string& value_) {
    key = key_;
    bl.append(value_);
  }
  RadosMetadata(enum rbox_metadata_key _key, const std::string& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const time_t& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const char* val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const uint& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const size_t& val) { convert(_key, val); }

 public:
  ceph::bufferlist& get_bl();
  std::string& get_key();

  void convert(const char* value, time_t* t) {
    if (t != NULL) {
      std::istringstream stream(value);
      stream >> *t;
    }
  }

 public:
  ceph::bufferlist bl;
  std::string key;

 private:
  void convert(enum rbox_metadata_key _key, const std::string& val) {
    key = enum_to_string(_key);
    bl.append(val);
  }

  void convert(enum rbox_metadata_key _key, const time_t& time) {
    key = enum_to_string(_key);
    bl.append(std::to_string(time));
  }

  void convert(enum rbox_metadata_key _key, char* value) {
    key = enum_to_string(_key);
    bl.append(value);
  }

  void convert(enum rbox_metadata_key _key, const uint& value) {
    key = enum_to_string(_key);
    bl.append(std::to_string(value));
  }

  void convert(enum rbox_metadata_key _key, const size_t& value) {
    key = enum_to_string(_key);
    bl.append(std::to_string(static_cast<int>(value)));
  }

  std::string enum_to_string(enum rbox_metadata_key _key) {
    std::string k(1, static_cast<char>(_key));
    return k;
  }
};
}  // end namespace
#endif /* SRC_LIBRMB_RADOS_METADATA_H_ */
