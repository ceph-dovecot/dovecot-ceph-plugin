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

typedef unsigned int uint;

namespace librmb {

class RadosMetadata {
 public:
  RadosMetadata() {}
  RadosMetadata(std::string& key_, std::string& value_) : key(key_) { bl.append(value_.c_str(), value_.length() + 1); }
  RadosMetadata(enum rbox_metadata_key _key, const std::string& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const time_t& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const char* val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const uint& val) { convert(_key, val); }

  RadosMetadata(enum rbox_metadata_key _key, const size_t& val) { convert(_key, val); }
  RadosMetadata(enum rbox_metadata_key _key, const int val) { convert(_key, val); }
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
  static bool from_string(const std::string& str, RadosMetadata* metadata) {
    std::stringstream ss(str);
    std::string item;
    std::vector<std::string> token;
    while (std::getline(ss, item, '=')) {
      token.push_back(item);
    }
    if (token.size() != 2 || metadata == nullptr) {
      return false;
    }
    metadata->key = token[0];
    metadata->bl.append(token[1]);
    return true;
  }
  std::string to_string() {
    std::stringstream str;
    str << key << "=" << bl.to_str().substr(0, bl.length() - 1);
    return str.str();
  }

 public:
  ceph::bufferlist bl;
  std::string key;

  void convert(enum rbox_metadata_key _key, const std::string& val) {
    bl.clear();
    key = librmb::rbox_metadata_key_to_char(_key);
    bl.append(val.c_str(), val.length() + 1);
  }

  void convert(enum rbox_metadata_key _key, const time_t& time) {
    bl.clear();
    key = librmb::rbox_metadata_key_to_char(_key);
    std::string time_ = std::to_string(time);
    bl.append(time_.c_str(), time_.length() + 1);
  }

  void convert(enum rbox_metadata_key _key, char* value) {
    bl.clear();
    key = librmb::rbox_metadata_key_to_char(_key);
    std::string str = value;
    bl.append(str.c_str(), str.length() + 1);
  }

  void convert(enum rbox_metadata_key _key, const uint& value) {
    bl.clear();
    key = librmb::rbox_metadata_key_to_char(_key);
    std::string val = std::to_string(value);
    bl.append(val.c_str(), val.length() + 1);
  }

  void convert(enum rbox_metadata_key _key, const size_t& value) {
    bl.clear();
    key = librmb::rbox_metadata_key_to_char(_key);
    std::string val = std::to_string(static_cast<int>(value));
    bl.append(val.c_str(), val.length() + 1);
  }
  void convert(enum rbox_metadata_key _key, const int value) {
    bl.clear();
    key = librmb::rbox_metadata_key_to_char(_key);
    std::string val = std::to_string(value);
    bl.append(val.c_str(), val.length() + 1);
  }
};
}  // end namespace
#endif /* SRC_LIBRMB_RADOS_METADATA_H_ */
