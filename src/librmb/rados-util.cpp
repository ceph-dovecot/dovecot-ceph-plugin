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
#include "rados-util.h"
#include <string>
#include <limits.h>
#include <sstream>

namespace librmb {

RadosUtils::RadosUtils() {}

RadosUtils::~RadosUtils() {}

bool RadosUtils::convert_str_to_time_t(const std::string &date, time_t *val) {
  struct tm tm = {0};
  if (strptime(date.c_str(), "%Y-%m-%d %H:%M:%S", &tm)) {
    tm.tm_isdst = -1;
    time_t t = mktime(&tm);
    *val = t;
    return true;
  }
  *val = 0;
  return false;
}

bool RadosUtils::convert_string_to_date(const std::string &date_string, std::string *date) {
  time_t t;
  if (convert_str_to_time_t(date_string, &t)) {
    *date = std::to_string(t);
    return true;
  }
  return false;
}

bool RadosUtils::is_numeric(const std::string &s) {
  std::string::const_iterator it = s.begin();
  while (it != s.end() && std::isdigit(*it)) {
    ++it;
  }
  return !s.empty() && it == s.end();
}

bool RadosUtils::is_date_attribute(const rbox_metadata_key &key) {
  return (key == RBOX_METADATA_OLDV1_SAVE_TIME || key == RBOX_METADATA_RECEIVED_TIME);
}

int RadosUtils::convert_time_t_to_str(const time_t &t, std::string *ret_val) {
  char buffer[256];
  struct tm timeinfo;
  localtime_r(&t, &timeinfo);
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  *ret_val = std::string(buffer);
  return 0;
}

bool RadosUtils::flags_to_string(const uint8_t &flags_, std::string *flags_str) {
  std::stringstream sstream;
  sstream << std::hex << flags_;
  sstream >> *flags_str;
  return true;
}

bool RadosUtils::string_to_flags(const std::string &flags_, uint8_t *flags) {
  std::istringstream in(flags_);

  if (in >> std::hex >> *flags) {
    return true;
  }
  return false;
}

void RadosUtils::find_and_replace(std::string *source, std::string const &find, std::string const &replace) {
  for (std::string::size_type i = 0; source != nullptr && (i = source->find(find, i)) != std::string::npos;) {
    source->replace(i, find.length(), replace);
    i += replace.length();
  }
}

int RadosUtils::get_all_keys_and_values(const librados::IoCtx *io_ctx, const std::string &oid,
                                        std::map<std::string, librados::bufferlist> *kv_map) {
  int err = 0;
  librados::ObjectReadOperation first_read;
  std::set<std::string> extended_keys;
#ifdef HAVE_OMAP_GET_KEYS_2
  first_read.omap_get_keys2("", LONG_MAX, &extended_keys, nullptr, &err);
#else
  first_read.omap_get_keys("", LONG_MAX, &extended_keys, &err);
#endif
  io_ctx->operate(oid.c_str(), &first_read, NULL);
  if (err < 0) {
    return err;
  }
  return io_ctx->omap_get_vals_by_keys(oid, extended_keys, kv_map);
}

}  // namespace librmb
