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

#ifndef SRC_LIBRMB_RADOS_MAIL_H_
#define SRC_LIBRMB_RADOS_MAIL_H_

#include <string>
#include <iostream>
#include <sstream>
#include <map>
#include "rados-metadata.h"
#include "rados-types.h"
#include <rados/librados.hpp>

namespace librmb {

using std::map;
using std::string;

using librados::AioCompletion;
using librados::ObjectWriteOperation;

/**
 * Rados mail object
 *
 * ceph mail representation.
 *
 */
class RadosMail {
 public:
  RadosMail();
  virtual ~RadosMail();
  void set_oid(const char* _oid) { this->oid = _oid; }
  void set_oid(const string& _oid) { this->oid = _oid; }
  void set_mail_size(const int _size) { object_size = _size; }
  void set_active_op(int num_write_op) { this->active_op = num_write_op; }
  void set_rados_save_date(const time_t& _save_date) { this->save_date_rados = _save_date; }

  string* get_oid() { return &this->oid; }
  int get_mail_size() { return this->object_size; }

  time_t get_rados_save_date() { return this->save_date_rados; }
  uint8_t get_guid_ref() { return *this->guid; }
  /*!
   * @return ptr to internal buffer .
   */
  librados::bufferlist* get_mail_buffer() { return &this->mail_buffer; }
  map<string, ceph::bufferlist>* get_metadata() { return &this->attrset; }

  AioCompletion* get_completion() { return completion; }

  ObjectWriteOperation* get_write_operation() { return write_operation; }
  void set_write_operation(ObjectWriteOperation* write_operation_) { this->write_operation = write_operation_; }
  void set_completion(AioCompletion* completion_) { this->completion = completion_; }

  /*!
   * @return reference to all write operations related with this object
   */

  /*  void get_metadata(const std::string& key, char** value) {
      if (attrset.find(key) != attrset.end()) {
        *value = attrset[key].c_str();
        return;
      }
      *value = NULL;
    }
    void get_metadata(rbox_metadata_key key, char** value) {
      string str_key(librmb::rbox_metadata_key_to_char(key));
      get_metadata(str_key, value);
    }*/

  bool is_index_ref() { return index_ref; }
  void set_index_ref(bool ref) { this->index_ref = ref; }
  bool is_valid() { return valid; }
  void set_valid(bool valid_) { valid = valid_; }
  bool has_active_op() { return active_op > 0; }
  int get_num_active_op() { return active_op; }
  string to_string(const string& padding);
  void add_metadata(const RadosMetadata& metadata) { attrset[metadata.key] = metadata.bl; }

  /*!
   * Some metadata isn't saved as xattribute (default). To access those, get_extended_metadata can
   * be used.
   */
  map<string, ceph::bufferlist>* get_extended_metadata() { return &this->extended_attrset; }
  /*!
   * Save metadata to extended metadata store currently omap
   * @param[in] metadata valid radosMetadata.
   */
  void add_extended_metadata(const RadosMetadata& metadata) { extended_attrset[metadata.key] = metadata.bl; }

  const string get_extended_metadata(const string& key) {
    if (extended_attrset.find(key) != extended_attrset.end()) {
      return extended_attrset[key].to_str();
    }
    return nullptr;
  }

 private:
  string oid;
  uint8_t guid[GUID_128_SIZE] = {};
  int object_size;  // byte
  AioCompletion* completion;
  ObjectWriteOperation* write_operation;

  int active_op;
  ceph::bufferlist mail_buffer;
  time_t save_date_rados;

  map<string, ceph::bufferlist> attrset;
  map<string, ceph::bufferlist> extended_attrset;
  bool valid;
  bool index_ref;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_MAIL_H_
