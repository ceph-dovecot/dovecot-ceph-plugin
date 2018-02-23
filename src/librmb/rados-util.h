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

#ifndef SRC_LIBRMB_RADOS_UTIL_H_
#define SRC_LIBRMB_RADOS_UTIL_H_

#include <string.h>
#include <time.h>
#include <stdlib.h>

#include <string>

#include "rados-types.h"

namespace librmb {

class RadosUtils {
 public:
  RadosUtils();
  virtual ~RadosUtils();

  static bool convert_str_to_time_t(const std::string &date, time_t *val);
  static bool is_numeric(const std::string &s);
  static bool is_date_attribute(const rbox_metadata_key &key);

  static bool convert_string_to_date(const std::string &date_string, std::string *date);
  static int convert_time_t_to_str(const time_t &t, std::string *ret_val);
  static bool flags_to_string(const uint8_t &flags, std::string *flags_str);
  static bool string_to_flags(const std::string &flags_str, uint8_t *flags);

  static void find_and_replace(std::string *source, std::string const &find, std::string const &replace);
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_UTIL_H_
