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

RadosMailObject::RadosMailObject() {
  memset(this->guid, 0, GUID_128_SIZE);
  this->object_size = -1;
  this->active_op = false;
  this->mail_buffer = NULL;
  this->save_date_rados = -1;
  this->mail_buffer_start = NULL;
}

void RadosMailObject::set_guid(const uint8_t *_guid) { memcpy(this->guid, _guid, sizeof(this->guid)); }

std::string RadosMailObject::to_string(const string &padding) {
  string uid = get_metadata(RBOX_METADATA_MAIL_UID);
  string recv_time_str = get_metadata(RBOX_METADATA_RECEIVED_TIME);
  string p_size = get_metadata(RBOX_METADATA_PHYSICAL_SIZE);
  string v_size = get_metadata(RBOX_METADATA_VIRTUAL_SIZE);

  string rbox_version = get_metadata(RBOX_METADATA_VERSION);
  string mailbox_guid = get_metadata(RBOX_METADATA_MAILBOX_GUID);
  string mail_guid = get_metadata(RBOX_METADATA_GUID);
  string mb_orig_name = get_metadata(RBOX_METADATA_ORIG_MAILBOX);

  time_t ts = -1;

  if (!recv_time_str.empty()) {
    ts = static_cast<time_t>(stol(recv_time_str));
  }
  ostringstream ss;

  ss << endl;
  ss << padding << "MAIL:   " << static_cast<char>(RBOX_METADATA_MAIL_UID) << "(uid)=" << uid << endl;
  ss << padding << "        "
     << "oid = " << oid << endl;
  string recv_time;
  RadosUtils::convert_time_t_to_str(ts, &recv_time);
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_RECEIVED_TIME) << "(receive_time)=" << recv_time
     << "\n";
  string save_time;
  RadosUtils::convert_time_t_to_str(save_date_rados, &save_time);
  ss << padding << "        "
     << "save_time=" << save_time << "\n";
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_PHYSICAL_SIZE) << "(phy_size)=" << p_size << " "
     << static_cast<char>(RBOX_METADATA_VIRTUAL_SIZE) << "(v_size) = " << v_size << " stat_size=" << object_size
     << endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_MAILBOX_GUID) << "(mailbox_guid)=" << mailbox_guid
     << endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_ORIG_MAILBOX) << "(mailbox_orig_name)=" << mb_orig_name
     << endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_GUID) << "(mail_guid)=" << mail_guid << endl;
  ss << padding << "        " << static_cast<char>(RBOX_METADATA_VERSION) << "(rbox_version): " << rbox_version << endl;

  return ss.str();
}
