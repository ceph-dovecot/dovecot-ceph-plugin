/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "rados-mail-object.h"

#include <cstring>

using namespace librados;  // NOLINT
using namespace librmb;    // NOLINT

const std::string RadosMailObject::X_ATTR_STATE = "STATE";
const std::string RadosMailObject::X_ATTR_STATE_VALUES[] = {"S", "F"};
const std::string RadosMailObject::X_ATTR_VERSION = "V";
const std::string RadosMailObject::X_ATTR_VERSION_VALUE = "0.1";


const std::string RadosMailObject::DATA_BUFFER_NAME = "RADOS_MAIL_BUFFER";

void RadosMailObject::set_guid(const uint8_t* guid) {
  std::memcpy(this->guid, guid, sizeof(this->guid));

}

RadosMailObject::RadosMailObject() {
  memset(this->guid, 0, GUID_128_SIZE);
  this->object_size = -1;
  this->active_op = false;
  this->mail_buffer = NULL;
}
