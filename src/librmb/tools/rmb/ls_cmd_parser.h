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

#ifndef SRC_LIBRMB_TOOLS_RMB_LS_CMD_PARSER_H_
#define SRC_LIBRMB_TOOLS_RMB_LS_CMD_PARSER_H_

#include <string>
#include <vector>
#include <iostream>
#include <ctime>
#include <map>
#include "rados-mail-object.h"
#include "rados-util.h"

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

namespace librmb {

class Predicate {
 public:
  std::string key;
  std::string op;
  std::string value;  // value to check against e.g. key > value
  bool valid;

  bool eval(const std::string &_p_value) {
    rbox_metadata_key rbox_key = static_cast<librmb::rbox_metadata_key>(*key.c_str());

    if (rbox_key == RBOX_METADATA_RECEIVED_TIME || rbox_key == RBOX_METADATA_OLDV1_SAVE_TIME) {
      // ref value
      time_t query_date = 0;
      convert_str_to_time_t(this->value, &query_date);

      uint64_t val2 = -1;
      try {
        val2 = std::stol(_p_value);
      } catch (std::exception &e) {
        std::cerr << "eval: search criteria: RBOX_METADATA_RECEIVED_TIME or RBOX_METADATA_OLDV1_SAVE_TIME '" << _p_value << "' is not a number " << std::endl;
      }
      time_t obj_date = static_cast<time_t>(val2);

      double diff = difftime(obj_date, query_date);
      // std::cout << " comparing : " << query_date << " " << obj_date << std::endl;
      if (this->op.compare("=") == 0) {
        return diff == 0;
      } else if (this->op.compare(">") == 0) {
        return diff > 0;
      } else {
        return diff < 0;
      }
      // time
      return true;
    } else if (rbox_key == RBOX_METADATA_VIRTUAL_SIZE || rbox_key == RBOX_METADATA_PHYSICAL_SIZE ||
               rbox_key == RBOX_METADATA_MAIL_UID) {
      uint64_t val = -1;
      uint64_t val2 = -1;
      try {
        val = std::stol(_p_value);
        val2 = std::stol(this->value);
      } catch (std::exception &e) {
        std::cerr << "eval: search criteria: RBOX_METADATA_VIRTUAL_SIZE or RBOX_METADATA_PHYSICAL_SIZE or RBOX_METADATA_MAIL_UID: _p_value " << _p_value << " or " << this->value << " is not a number" << std::endl;
      }

      if (this->op.compare("=") == 0) {
        // numeric
        return val == val2;
      } else if (this->op.compare(">") == 0) {
        return val > val2;
      } else {
        return val < val2;
      }

    } else {
      // string
      // std::cout << " comparing : " << this->value << " with " << _p_value << std::endl;
      return this->value.compare(_p_value) == 0;
    }
    return false;
  }

  bool convert_str_to_time_t(const std::string &date, time_t *val) {
    return librmb::RadosUtils::convert_str_to_time_t(date, val);
  }
  int convert_time_t_to_str(const time_t &t, std::string *ret_val) {
    return librmb::RadosUtils::convert_time_t_to_str(t, ret_val);
  }
};

class CmdLineParser {
 public:
  explicit CmdLineParser(const std::string &_ls_value) {
    size_t pos = ls_value.find("\"");
    if (pos != std::string::npos) {
      this->ls_value = _ls_value.substr(1, _ls_value.length() - 1);
    }
    this->ls_value = _ls_value;
  }
  ~CmdLineParser();
  bool parse_ls_string();
  std::map<std::string, Predicate *> &get_predicates() { return this->predicates; }

  bool contains_key(const std::string &key) { return keys.find(key) != keys.npos ? true : false; }
  Predicate *get_predicate(const std::string &key) { return predicates[key]; }
  Predicate *create_predicate(const std::string &ls_value);

  void set_output_dir(const std::string out);
  std::string &get_output_dir() { return this->out_dir; }

 private:
  std::map<std::string, Predicate *> predicates;
  std::string ls_value;
  std::string keys;
  std::string out_dir;
};
};      // namespace librmb
#endif  // SRC_LIBRMB_TOOLS_RMB_LS_CMD_PARSER_H_
