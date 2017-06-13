/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
#define SRC_LIBRMB_RADOS_MAIL_OBJECT_H_

#include <string>
#include <iostream>
#include <sstream>
#include <rados/librados.hpp>

#define GUID_128_SIZE 16

namespace librmb {

typedef std::shared_ptr<librados::AioCompletion> AioCompletionPtr;

class RadosMailObject {
 public:
  RadosMailObject();
  virtual ~RadosMailObject() {}

  void set_oid(const char* oid) { this->oid = oid; }
  void set_oid(const std::string& oid) { this->oid = oid; }
  void set_state(const std::string& state) { this->state = state; }
  void set_version(const std::string& version) { this->version = version; }

  void set_pop3_order(const uint32_t& pop3_order) { this->pop3_order = pop3_order; }
  void set_pop3_uidl(const std::string& pop3_uidl) { this->pop3_uidl = pop3_uidl; }
  void set_save_date(const time_t& save_date) { this->save_date = save_date; }
  void set_received_date(const time_t& received_date) { this->received_date = received_date; }
  void set_guid(const uint8_t* guid);

  void update_bytes_written(uint64_t& bytes_read) { this->bytes_written += bytes_read; }

  const std::string get_oid() { return this->oid; }
  const std::string get_state() { return state; }
  const std::string get_initial_state_value() { return X_ATTR_STATE_VALUES[0]; }
  const std::string get_finised_state_value() { return X_ATTR_STATE_VALUES[1]; }
  const std::string get_version() { return this->version; }

  const uint32_t get_pop3_order() { return pop3_order; }
  const std::string get_pop3_uidl() { return pop3_uidl; }
  const time_t get_save_date() { return save_date; }
  const time_t get_received_date() { return received_date; }
  uint8_t* get_guid_ref() { return guid; }

  std::string& get_mail_data_ref() { return mail_data; }
  const uint64_t get_bytes_written() { return this->bytes_written; }

  librados::ObjectWriteOperation& get_write_op() { return this->write_op; }
  librados::ObjectReadOperation& get_read_op() { return this->read_op; }

  AioCompletionPtr get_completion_private() { return this->completion_private; }
  void rados_transaction_private_complete_callback(rados_completion_t comp, void* arg);
  bool is_aio_write_successful() { return this->aio_write_successful; }
  bool is_aio_write_finished() { return this->aio_write_finished; }

 private:
  std::string oid;
  std::string state;
  std::string version;
  std::string mail_data;
  uint32_t pop3_order;
  std::string pop3_uidl;
  time_t save_date;
  time_t received_date;
  uint8_t guid[GUID_128_SIZE];
  uint64_t bytes_written;

  librados::ObjectWriteOperation write_op;
  librados::ObjectReadOperation read_op;

  AioCompletionPtr completion_private;
  bool aio_write_successful;
  bool aio_write_finished;

 public:
  // X_ATTRIBUTES
  static const std::string X_ATTR_STATE;
  static const std::string X_ATTR_STATE_VALUES[];
  static const std::string X_ATTR_VERSION;
  static const std::string X_ATTR_VERSION_VALUE;

  static const std::string X_ATTR_GUID;
  static const std::string X_ATTR_RECEIVED_DATE;
  static const std::string X_ATTR_SAVE_DATE;
  static const std::string X_ATTR_POP3_UIDL;
  static const std::string X_ATTR_POP3_ORDER;

  // OTHER
  static const std::string DATA_BUFFER_NAME;
};

}  // namespace librmb
#endif  // SRC_LIBRMB_RADOS_MAIL_OBJECT_H_
