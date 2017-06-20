/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "rados-mail-object.h"

#include <cstring>

using namespace librados;  // NOLINT
using namespace librmb;    // NOLINT

const std::string RadosMailObject::X_ATTR_STATE = "STATE";
const std::string RadosMailObject::X_ATTR_STATE_VALUES[] = {"S", "F"};
const std::string RadosMailObject::X_ATTR_VERSION = "VERSION";
const std::string RadosMailObject::X_ATTR_VERSION_VALUE = "0.1";

const std::string RadosMailObject::X_ATTR_GUID = "G";
const std::string RadosMailObject::X_ATTR_RECEIVED_DATE = "R";
const std::string RadosMailObject::X_ATTR_SAVE_DATE = "S";
const std::string RadosMailObject::X_ATTR_POP3_UIDL = "P";
const std::string RadosMailObject::X_ATTR_POP3_ORDER = "O";

const std::string RadosMailObject::DATA_BUFFER_NAME = "RADOS_MAIL_BUFFER";

void RadosMailObject::set_guid(const uint8_t* guid) {
  std::memcpy(this->guid, guid, sizeof(this->guid));

}

RadosMailObject::RadosMailObject() {
  this->pop3_order = 0;
  this->save_date = 0;
  this->received_date = 0;
  memset(this->guid, 0, GUID_128_SIZE);
  completion_private = std::make_shared<librados::AioCompletion>(*librados::Rados::aio_create_completion());
  aio_write_successful = true;
  aio_write_finished = false;
}

// exclusive use for write operations (not sure if radps_completion_t can be something else than int
void RadosMailObject::rados_transaction_private_complete_callback(rados_completion_t comp, void* arg) {
  int ret_val = (int)comp;

  if (ret_val < 0) {
    aio_write_successful = false;
  }
  aio_write_finished = true;
}
