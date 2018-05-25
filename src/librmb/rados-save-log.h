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

#ifndef SRC_LIBRMB_RADOS_SAVE_LOG_H_
#define SRC_LIBRMB_RADOS_SAVE_LOG_H_

#include <fstream>  // std::ofstream
#include <regex>
namespace librmb {

class RadosSaveLogEntry {
 public:
  RadosSaveLogEntry() {}
  RadosSaveLogEntry(const std::string &oid_, const std::string &ns_, const std::string &pool_) {
    this->oid = oid_;
    this->ns = ns_;
    this->pool = pool_;
  }
  ~RadosSaveLogEntry(){};

  friend std::ostream &operator<<(std::ostream &os, const RadosSaveLogEntry &obj) {
    os << obj.oid << "," << obj.ns << "," << obj.pool << std::endl;
    return os;
  }

  friend std::istream &operator>>(std::istream &is, RadosSaveLogEntry &obj) {
    std::string item;
    const std::regex re{"((?:[^\\\\,]|\\\\.)*?)(?:,|$)"};
    std::getline(is, item);
    std::vector<std::string> csv_items{std::sregex_token_iterator(item.begin(), item.end(), re, 1),
                                       std::sregex_token_iterator()};
    // read obj from stream
    if (csv_items.size() < 3) {
      is.setstate(std::ios::failbit);
    } else {
      obj.oid = csv_items[0];
      obj.ns = csv_items[1];
      obj.pool = csv_items[2];
    }
    return is;
  }

 public:
  std::string oid;   // oid
  std::string ns;    // namespace
  std::string pool;  // storage pool
};

class RadosSaveLog {
 public:
  RadosSaveLog(const std::string &logfile_) {
    this->logfile = logfile_;
    log_active = !logfile.empty();
  }
  virtual ~RadosSaveLog(){};
  bool open();
  void append(const RadosSaveLogEntry &entry);
  bool close();

 private:
  std::string logfile;
  bool log_active;
  std::ofstream ofs;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_SAVE_LOG_H_ */
