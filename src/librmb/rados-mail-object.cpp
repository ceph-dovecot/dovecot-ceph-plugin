/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "rados-mail-object.h"

#include <cstring>
#include <sstream>
#include <vector>
#include <stdlib.h>
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
  this->save_date_rados = 0;
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

std::string RadosMailObject::to_string(std::string &padding) {
  std::string uid = get_xvalue(RBOX_METADATA_MAIL_UID);
  std::string recv_time_str = get_xvalue(RBOX_METADATA_RECEIVED_TIME);
  std::string p_size = get_xvalue(RBOX_METADATA_PHYSICAL_SIZE);
  std::string v_size = get_xvalue(RBOX_METADATA_VIRTUAL_SIZE);

  std::string rbox_version = get_xvalue(RBOX_METADATA_VERSION);
  std::string mailbox_guid = get_xvalue(RBOX_METADATA_MAILBOX_GUID);
  std::string mail_guid = get_xvalue(RBOX_METADATA_GUID);

  time_t ts = static_cast<time_t>(std::stol(recv_time_str));

  long object_i = std::stol(p_size);
  long object_v = std::stol(v_size);

  std::ostringstream ss;

  ss << std::endl;
  ss << padding << "MAIL:   " << (char)RBOX_METADATA_MAIL_UID << "(uid)=" << uid << std::endl;
  ss << padding << "        "
     << "oid = " << oid << std::endl;
  ss << padding << "        " << (char)RBOX_METADATA_RECEIVED_TIME << "(receive_time)=" << std::ctime(&ts);

  ss << padding << "        "
     << "save_time=" << std::ctime(&save_date_rados);
  ss << padding << "        " << (char)RBOX_METADATA_PHYSICAL_SIZE << "(phy_size)=" << object_i << " "
     << (char)RBOX_METADATA_VIRTUAL_SIZE << "(v_size) = " << object_v << " stat_size=" << object_size << std::endl;
  ss << padding << "        " << (char)RBOX_METADATA_MAILBOX_GUID << "(mailbox_guid)=" << mailbox_guid << std::endl;
  ss << padding << "        " << (char)RBOX_METADATA_GUID << "(mail_guid)=" << mail_guid << std::endl;
  ss << padding << "        " << (char)RBOX_METADATA_VERSION << "(rbox_version): " << rbox_version << std::endl;

  return ss.str();
}
