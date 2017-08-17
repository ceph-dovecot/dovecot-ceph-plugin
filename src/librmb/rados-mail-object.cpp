/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "rados-mail-object.h"

#include <cstring>

using namespace librados;  // NOLINT
using namespace librmb;    // NOLINT


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

bool RadosMailObject::wait_for_write_operations_complete() {
  bool ctx_failed = false;

  for (std::map<librados::AioCompletion *, librados::ObjectWriteOperation *>::iterator map_it =
           get_completion_op_map()->begin();
       map_it != get_completion_op_map()->end(); ++map_it) {
    map_it->first->wait_for_complete_and_cb();
    ctx_failed = map_it->first->get_return_value() < 0 || ctx_failed ? true : false;
    // clean up
    map_it->first->release();
    map_it->second->remove();
    delete map_it->second;
  }
  return ctx_failed;
}
