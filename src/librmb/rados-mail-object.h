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

#ifndef SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
#define SRC_LIBRMB_RADOS_MAIL_OBJECT_H_

#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include "rados-metadata.h"
#include "rados-types.h"
#include <rados/librados.hpp>

#define GUID_128_SIZE 16

namespace librmb {




class RadosMailObject {
 public:
  RadosMailObject();
  virtual ~RadosMailObject() {}
  void set_oid(const char* _oid) { this->oid = _oid; }
  void set_oid(const std::string& _oid) { this->oid = _oid; }
  void set_state(const std::string& _state) { this->state = _state; }
  void set_version(const std::string& _version) { this->version = _version; }

  void set_guid(const uint8_t* guid);

  const std::string get_oid() { return this->oid; }
  const std::string get_version() { return this->version; }

  uint8_t* get_guid_ref() { return guid; }

  const uint64_t& get_object_size() { return this->object_size; }

  void set_object_size(const uint64_t& _size) { object_size = _size; }

  bool has_active_op() { return active_op; }
  void set_active_op(bool _active) { this->active_op = _active; }
  std::map<librados::AioCompletion*, librados::ObjectWriteOperation*>* get_completion_op_map() {
    return &completion_op;
  }
  void set_mail_buffer(char* _mail_buffer) { this->mail_buffer = _mail_buffer; }
  char* get_mail_buffer() { return this->mail_buffer; }

  std::map<std::string, ceph::bufferlist>* get_xattr() { return &this->attrset; }

  std::string get_xvalue(rbox_metadata_key key) {
    std::string str_key(1, static_cast<char>(key));
    return get_xvalue(str_key);
  }
  const std::string get_xvalue(std::string key) {
    std::string value;
    if (attrset.find(key) != attrset.end()) {
      value = attrset[key].to_str();
    }
    return value;
  }

  std::string to_string(const std::string& padding);
  void set_rados_save_date(const time_t& _save_date) { this->save_date_rados = _save_date; }
  time_t* get_rados_save_date() { return &this->save_date_rados; }

 private:
  std::string oid;

  // XATTR
  std::string state;
  std::string version;

  uint8_t guid[GUID_128_SIZE];
  uint64_t object_size;  // byte
  std::map<librados::AioCompletion*, librados::ObjectWriteOperation*> completion_op;

  bool active_op;
  // used as pointer to a buffer_t (to avoid using dovecot datatypes in library)
  char* mail_buffer;
  time_t save_date_rados;

  std::map<std::string, ceph::bufferlist> attrset;

 public:
  // X_ATTRIBUTES
  static const char X_ATTR_VERSION_VALUE[];

  // OTHER
  static const char DATA_BUFFER_NAME[];
};

}  // namespace librmb
#endif  // SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
