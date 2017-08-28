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

enum rbox_metadata_key {
  /*
   * mailbox global unique id the mail currently is in.
   */
  RBOX_METADATA_MAILBOX_GUID = 'M',
  /* Globally unique identifier for the message. Preserved when
     copying. */
  RBOX_METADATA_GUID = 'G',
  /* POP3 UIDL overriding the default format */
  RBOX_METADATA_POP3_UIDL = 'P',
  /* POP3 message ordering (for migrated mails) */
  RBOX_METADATA_POP3_ORDER = 'O',
  /* Received UNIX timestamp in hex */
  RBOX_METADATA_RECEIVED_TIME = 'R',
  /* Physical message size in hex. Necessary only if it differs from
     the dbox_message_header.message_size_hex, for example because the
     message is compressed. */
  RBOX_METADATA_PHYSICAL_SIZE = 'Z',
  /* Virtual message size in hex (line feeds counted as CRLF) */
  RBOX_METADATA_VIRTUAL_SIZE = 'V',
  /* Pointer to external message data. Format is:
     1*(<start offset> <byte count> <options> <ref>) */
  RBOX_METADATA_EXT_REF = 'X',
  /* Mailbox name where this message was originally saved to.
     When rebuild finds a message whose mailbox is unknown, it's
     placed to this mailbox. */
  RBOX_METADATA_ORIG_MAILBOX = 'B',

  RBOX_METADATA_MAIL_UID = 'U',
  RBOX_METADATA_VERSION = 'I',
  /*
   * Mails from envelope
   */
  RBOX_METADATA_FROM_ENVELOPE = 'A',
  RBOX_METADATA_PVT_FLAGS = 'C',
  /* metadata used by old Dovecot versions */
  RBOX_METADATA_OLDV1_EXPUNGED = 'E',
  RBOX_METADATA_OLDV1_FLAGS = 'F',
  RBOX_METADATA_OLDV1_KEYWORDS = 'K',
  RBOX_METADATA_OLDV1_SAVE_TIME = 'S',
  RBOX_METADATA_OLDV1_SPACE = ' '
};

class RadosXAttr {
 public:
  RadosXAttr() {}

 public:
  ceph::bufferlist bl;
  std::string key;

 public:
  static void convert(enum rbox_metadata_key key, const std::string& val, RadosXAttr* attr) {
    attr->key = enum_to_string(key);
    attr->bl.append(val);
  }
  static void convert(enum rbox_metadata_key key, time_t* time, RadosXAttr* attr) {
    attr->key = enum_to_string(key);
    long ts = static_cast<long int>(*time);
    attr->bl.append(std::to_string(ts));
  }
  static void convert(enum rbox_metadata_key key, char* value, RadosXAttr* attr) {
    attr->key = enum_to_string(key);
    attr->bl.append(value);
  }

  static void convert(enum rbox_metadata_key key, uint& value, RadosXAttr* attr) {
    attr->key = enum_to_string(key);
    attr->bl.append(std::to_string(value));
  }
  static void convert(enum rbox_metadata_key key, size_t& value, RadosXAttr* attr) {
    attr->key = enum_to_string(key);
    attr->bl.append(std::to_string((int)value));
  }

  static void convert(const char* value, time_t& time) {
    long ts = std::stol(value);
    time = static_cast<time_t>(ts);
  }

 private:
  static std::string enum_to_string(enum rbox_metadata_key key) {
    std::string k(1, (char)key);
    return k;
  }
};

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

  void set_object_size(uint64_t& _size) { this->object_size = _size; }

  bool has_active_op() { return active_op; }
  void set_active_op(bool _active) { this->active_op = _active; }
  std::map<librados::AioCompletion*, librados::ObjectWriteOperation*>* get_completion_op_map() {
    return &completion_op;
  }
  void set_mail_buffer(char* _mail_buffer) { this->mail_buffer = _mail_buffer; }
  char* get_mail_buffer() { return this->mail_buffer; }

  bool wait_for_write_operations_complete();

  std::map<std::string, ceph::bufferlist>* get_xattr() { return &this->attrset; }

  std::string get_xvalue(rbox_metadata_key key) {
    std::string str_key(1, (char)key);
    return get_xvalue(str_key);
  }
  const std::string get_xvalue(std::string key) {
    std::string value;
    if (attrset.find(key) != attrset.end()) {
      value = attrset[key].to_str();
    }
    return value;
  }

  std::string to_string(std::string& padding);
  void set_rados_save_date(time_t& _save_date) { this->save_date_rados = _save_date; }
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
  static const std::string X_ATTR_VERSION_VALUE;

  // OTHER
  static const std::string DATA_BUFFER_NAME;
};

}  // namespace librmb
#endif  // SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
