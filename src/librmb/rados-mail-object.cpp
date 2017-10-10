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

#include "rados-mail-object.h"

#include <stdlib.h>

#include <cstring>
#include <sstream>
#include <vector>
#include "rados-util.h"

using librmb::RadosMailObject;

const char RadosMailObject::X_ATTR_VERSION_VALUE[] = "0.1";
const char RadosMailObject::DATA_BUFFER_NAME[] = "RADOS_MAIL_BUFFER";

void RadosMailObject::set_guid(const uint8_t *_guid) { std::memcpy(this->guid, _guid, sizeof(this->guid)); }

RadosMailObject::RadosMailObject() {
  memset(this->guid, 0, GUID_128_SIZE);
  this->object_size = -1;
  this->active_op = false;
  this->mail_buffer = NULL;
  this->save_date_rados = 0;
}

std::string RadosMailObject::to_string(const std::string &padding) {
  std::string uid = get_xvalue(RBOX_METADATA_MAIL_UID);
  std::string recv_time_str = get_xvalue(RBOX_METADATA_RECEIVED_TIME);
  std::string p_size = get_xvalue(RBOX_METADATA_PHYSICAL_SIZE);
  std::string v_size = get_xvalue(RBOX_METADATA_VIRTUAL_SIZE);

  std::string rbox_version = get_xvalue(RBOX_METADATA_VERSION);
  std::string mailbox_guid = get_xvalue(RBOX_METADATA_MAILBOX_GUID);
  std::string mail_guid = get_xvalue(RBOX_METADATA_GUID);
  std::string mb_orig_name = get_xvalue(RBOX_METADATA_ORIG_MAILBOX);

  time_t ts = -1;

  if (!recv_time_str.empty()) {
    ts = static_cast<time_t>(std::stol(recv_time_str));
  }
  std::ostringstream ss;

  ss << std::endl;
  ss << padding << "MAIL:   " << static_cast<char>(RBOX_METADATA_MAIL_UID) << "(uid)=" << uid << std::endl;
  ss << padding << "        "
     << "oid = " << oid << std::endl;
  std::string recv_time;
  RadosUtils::convert_time_t_to_str(ts, &recv_time);
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_RECEIVED_TIME) << "(receive_time)=" << recv_time
     << "\n";
  std::string save_time;
  RadosUtils::convert_time_t_to_str(save_date_rados, &save_time);
  ss << padding << "        "
     << "save_time=" << save_time << "\n";
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_PHYSICAL_SIZE) << "(phy_size)=" << p_size << " "
     << static_cast<char>(RBOX_METADATA_VIRTUAL_SIZE) << "(v_size) = " << v_size << " stat_size=" << object_size
     << std::endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_MAILBOX_GUID) << "(mailbox_guid)=" << mailbox_guid
     << std::endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_ORIG_MAILBOX) << "(mailbox_orig_name)=" << mb_orig_name
     << std::endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_GUID) << "(mail_guid)=" << mail_guid << std::endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_VERSION) << "(rbox_version): " << rbox_version
     << std::endl;

  return ss.str();
}
