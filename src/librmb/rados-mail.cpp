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

#include "rados-mail.h"

#include <stdlib.h>

#include <cstring>
#include <sstream>
#include <vector>
#include "rados-util.h"

using std::endl;
using std::ostringstream;

using librmb::RadosMail;

RadosMail::RadosMail() : object_size(-1), active_op(false), save_date_rados(-1), valid(true), index_ref(false) {}

RadosMail::~RadosMail() {}

std::string RadosMail::to_string(const string& padding) {
  char* uid;
  get_metadata(RBOX_METADATA_MAIL_UID, &uid);
  char* recv_time_str;
  get_metadata(RBOX_METADATA_RECEIVED_TIME, &recv_time_str);
  char* p_size;
  get_metadata(RBOX_METADATA_PHYSICAL_SIZE, &p_size);
  char* v_size;
  get_metadata(RBOX_METADATA_VIRTUAL_SIZE, &v_size);

  char* rbox_version;
  get_metadata(RBOX_METADATA_VERSION, &rbox_version);
  char* mailbox_guid;
  get_metadata(RBOX_METADATA_MAILBOX_GUID, &mailbox_guid);
  char* mail_guid;
  get_metadata(RBOX_METADATA_GUID, &mail_guid);
  char* mb_orig_name;
  get_metadata(RBOX_METADATA_ORIG_MAILBOX, &mb_orig_name);

  // string keywords = get_metadata(RBOX_METADATA_OLDV1_KEYWORDS);
  char* flags;
  get_metadata(RBOX_METADATA_OLDV1_FLAGS, &flags);
  char* pvt_flags;
  get_metadata(RBOX_METADATA_PVT_FLAGS, &pvt_flags);
  char* from_envelope;
  get_metadata(RBOX_METADATA_FROM_ENVELOPE, &from_envelope);

  time_t ts = -1;
  if (recv_time_str != NULL) {
    try {
      std::string recv(recv_time_str);
      ts = static_cast<time_t>(stol(recv));
    } catch (std::exception& ex) {
      ts = -1;
    }
  }
  ostringstream ss;
  ss << endl;
  if (!valid) {
    ss << padding << "<<<   MAIL OBJECT IS NOT VALID <<<<" << endl;
  }
  if (!index_ref) {
    ss << padding << "<<<   MAIL OBJECT HAS NO INDEX REFERENCE <<<<" << endl;
  }
  ss << padding << "MAIL:   ";
  if (uid != NULL) {
    ss << static_cast<char>(RBOX_METADATA_MAIL_UID) << "(uid)=" << uid << endl;
    ss << padding << "        ";
  }
  ss << "oid = " << oid << endl;
  string recv_time;
  if (RadosUtils::convert_time_t_to_str(ts, &recv_time) >= 0) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_RECEIVED_TIME) << "(receive_time)=" << recv_time
       << "\n";
  } else {
    if (recv_time_str != NULL) {
      ss << padding << "        " << static_cast<char>(RBOX_METADATA_RECEIVED_TIME)
         << "(receive_time)= INVALID DATE : '" << recv_time_str << "'"
         << "\n";
    }
  }
  string save_time;
  if (RadosUtils::convert_time_t_to_str(save_date_rados, &save_time) >= 0) {
    ss << padding << "        "
       << "save_time=" << save_time << "\n";
  } else {
    ss << padding << "        "
       << "save_time= INVALID DATE '" << save_date_rados << "'\n";
  }

  ss << padding << "        " << static_cast<char>(RBOX_METADATA_PHYSICAL_SIZE) << "(phy_size)=" << p_size << " "
     << static_cast<char>(RBOX_METADATA_VIRTUAL_SIZE) << "(v_size) = " << v_size << " stat_size=" << object_size
     << endl;
  if (mailbox_guid != NULL) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_MAILBOX_GUID) << "(mailbox_guid)=" << mailbox_guid
       << endl;
  }
  if (mb_orig_name != NULL) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_ORIG_MAILBOX)
       << "(mailbox_orig_name)=" << mb_orig_name << endl;
  }
  if (mail_guid != NULL) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_GUID) << "(mail_guid)=" << mail_guid << endl;
  }
  if (rbox_version != NULL) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_VERSION) << "(rbox_version): " << rbox_version
       << endl;
  }

  if (extended_attrset.size() > 0) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_OLDV1_KEYWORDS) << "(keywords): " << std::endl;
    for (std::map<string, ceph::bufferlist>::iterator iter = extended_attrset.begin(); iter != extended_attrset.end();
         ++iter) {
      ss << "                             " << iter->first << " : " << iter->second.to_str() << endl;
    }
  }

  if (flags == NULL) {
    uint8_t flags_;
    if (RadosUtils::string_to_flags(flags, &flags_)) {
      std::string resolved_flags;
      RadosUtils::resolve_flags(flags_, &resolved_flags);
      ss << padding << "        " << static_cast<char>(RBOX_METADATA_OLDV1_FLAGS) << "(flags): " << resolved_flags
         << std::endl;
    }
  }

  if (pvt_flags == NULL) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_PVT_FLAGS) << "(private flags): " << pvt_flags
       << endl;
  }

  if (from_envelope == NULL) {
    ss << padding << "        " << static_cast<char>(RBOX_METADATA_FROM_ENVELOPE)
       << "(from envelope): " << from_envelope << endl;
  }

  return ss.str();
}
