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

#ifndef SRC_LIBRMB_RADOS_METADATA_H_
#define SRC_LIBRMB_RADOS_METADATA_H_

#include "rados-types.h"
#include <string>
#include "time.h"
#include <stdlib.h>
#include <sstream>
#include <rados/librados.hpp>

namespace librmb {

class RadosMetadata {
 public:
  RadosMetadata() {}
  RadosMetadata(std::string& key_, std::string& value_) {
    key = key_;
    bl.append(value_);
  }
  RadosMetadata(enum rbox_metadata_key _key, const std::string& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const time_t& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const char* val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const uint& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const size_t& val) { convert(_key, val); }
  ~RadosMetadata() {}

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

  void convert(enum rbox_metadata_key _key, const std::string& val) {
    bl.clear();
    key = static_cast<char>(_key);
    bl.append(val);
  }

  void convert(enum rbox_metadata_key _key, const time_t& time) {
    bl.clear();
    key = static_cast<char>(_key);
    bl.append(std::to_string(time));
  }

  void convert(enum rbox_metadata_key _key, char* value) {
    bl.clear();
    key = static_cast<char>(_key);
    bl.append(value);
  }

  void convert(enum rbox_metadata_key _key, const uint& value) {
    bl.clear();
    key = static_cast<char>(_key);
    bl.append(std::to_string(value));
  }

  void convert(enum rbox_metadata_key _key, const size_t& value) {
    bl.clear();
    key = static_cast<char>(_key);
    bl.append(std::to_string(static_cast<int>(value)));
  }
};
}  // end namespace
#endif /* SRC_LIBRMB_RADOS_METADATA_H_ */
