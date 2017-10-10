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
#include "rados-util.h"

namespace librmb {

RadosUtils::RadosUtils() {
  // TODO Auto-generated constructor stub
}

RadosUtils::~RadosUtils() {
  // TODO Auto-generated destructor stub
}

bool RadosUtils::convert_str_to_time_t(const std::string &date, time_t *val) {
  struct tm tm;
  memset(&tm, 0, sizeof(struct tm));
  if (strptime(date.c_str(), "%Y-%m-%d %H:%M:%S", &tm)) {
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    *val = t;
    return true;
  }

  val = 0;
  return false;
}

std::string RadosUtils::convert_string_to_date(std::string &date) {
  std::string ret;
  time_t t;
  return convert_str_to_time_t(date, &t) ? std::to_string(t) : "";
}

bool RadosUtils::is_numeric(const std::string &s) {
  std::string::const_iterator it = s.begin();
  while (it != s.end() && std::isdigit(*it)) {
    ++it;
  }
  return !s.empty() && it == s.end();
}

bool RadosUtils::is_date_attribute(rbox_metadata_key &key) {
  return (key == RBOX_METADATA_OLDV1_SAVE_TIME || key == RBOX_METADATA_RECEIVED_TIME);
}
int RadosUtils::convert_time_t_to_str(const time_t &t, std::string *ret_val) {
  char buffer[256];
  struct tm *timeinfo;
  timeinfo = localtime(&t);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
  *ret_val = std::string(buffer);
  // std::stringstream buffer;
  // buffer << std::put_time(t, "%a %b %d %H:%M:%S %Y");
  //*ret_val = std::ctime(&t);
  //*ret_val = buffer.str();
  return 0;
}
} /* namespace tallence */
