/*
 * cmd_line_parser.h
 *
 *  Created on: Aug 22, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_TOOLS_RMB_LS_CMD_PARSER_H_
#define SRC_LIBRMB_TOOLS_RMB_LS_CMD_PARSER_H_

#include <string>
#include <vector>
#include "rados-mail-object.h"
#include <iostream>
#include <ctime>

namespace librmb {

class Predicate {
 public:
  std::string key;
  std::string op;
  std::string value;  // value to check against e.g. key > value
  bool valid;

  bool eval(const std::string &value) {
    rbox_metadata_key rbox_key = static_cast<librmb::rbox_metadata_key>(*key.c_str());

    if (rbox_key == RBOX_METADATA_RECEIVED_TIME || rbox_key == RBOX_METADATA_OLDV1_SAVE_TIME) {
      // ref value
      time_t query_date = 0;
      convert_str_to_time_t(this->value, &query_date);

      long val2 = std::stol(value);
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
      uint64_t val = std::stol(value);
      uint64_t val2 = std::stol(this->value);

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
      return this->value.compare(value) == 0;
    }
    return false;
  }

  bool convert_str_to_time_t(std::string &date, time_t *val) {
    struct tm tm;
    memset(&tm, 0, sizeof(struct tm));
    if (strptime(date.c_str(), "%Y-%m-%d %H:%M", &tm)) {
      tm.tm_isdst = -1;
      time_t t = mktime(&tm);  // t is now your desired time_t
      *val = t;
      return true;
    }

    val = 0;
    return false;
  }
  int convert_time_t_to_str(time_t &t, std::string &ret_val) {
    ret_val = std::ctime(&t);
    return 0;
  }
};

class CmdLineParser {
 public:
  CmdLineParser(std::string &ls_value) {
    size_t pos = ls_value.find("\"");
    if (pos != std::string::npos) {
      this->ls_value = ls_value.substr(1, ls_value.length() - 1);
    }
    this->ls_value = ls_value;
  };
  bool parse_ls_string();
  std::map<std::string, Predicate *> &get_predicates() { return this->predicates; }

  bool contains_key(std::string &key) { return keys.find(key) != keys.npos ? true : false; }
  Predicate *get_predicate(std::string &key) { return predicates[key]; }
  Predicate *create_predicate(std::string &ls_value);

 private:
  std::map<std::string, Predicate *> predicates;
  std::string ls_value;
  std::string keys;
};
};     // end namespace rmb
#endif /* SRC_LIBRMB_TOOLS_RMB_LS_CMD_PARSER_H_ */
