/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
#define SRC_LIBRMB_RADOS_MAIL_OBJECT_H_

#include <string>
#include <iostream>
#include <sstream>
#include <rados/librados.hpp>
#include <map>
#define GUID_128_SIZE 16

namespace librmb {

class RadosMailObject {
 public:
  RadosMailObject();
  virtual ~RadosMailObject() {

  }

  void set_oid(const char* oid) { this->oid = oid; }
  void set_oid(const std::string& oid) { this->oid = oid; }
  void set_state(const std::string& state) { this->state = state; }
  void set_version(const std::string& version) { this->version = version; }

  void set_guid(const uint8_t* guid);

  const std::string get_oid() { return this->oid; }
  const std::string get_state() { return state; }
  const std::string get_initial_state_value() { return X_ATTR_STATE_VALUES[0]; }
  const std::string get_finised_state_value() { return X_ATTR_STATE_VALUES[1]; }
  const std::string get_version() { return this->version; }

  uint8_t* get_guid_ref() { return guid; }

  const uint64_t get_object_size() { return this->object_size; }

  void set_object_size(uint64_t& size) { this->object_size = size; }

  bool has_active_op() { return active_op; }
  void set_active_op(bool active) { this->active_op = active; }
  std::map<librados::AioCompletion*, librados::ObjectWriteOperation*>* get_completion_op_map() {
    return &completion_op;
  }
  void set_mail_buffer(char* mail_buffer) { this->mail_buffer = mail_buffer; }
  char* get_mail_buffer() { return this->mail_buffer; }

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

 public:
  // X_ATTRIBUTES
  static const std::string X_ATTR_STATE;
  static const std::string X_ATTR_STATE_VALUES[];
  static const std::string X_ATTR_VERSION;
  static const std::string X_ATTR_VERSION_VALUE;


  // OTHER
  static const std::string DATA_BUFFER_NAME;
};

}  // namespace librmb
#endif  // SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
