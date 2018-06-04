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
//#include <regex>
#include <cstdio>
#include <vector>
#include <sstream>

namespace librmb {

class RadosSaveLogEntry {
 public:
  RadosSaveLogEntry() {}
  RadosSaveLogEntry(const std::string &oid_, const std::string &ns_, const std::string &pool_, const std::string &op_)
      : oid(oid_), ns(ns_), pool(pool_), op(op_) {}
  ~RadosSaveLogEntry(){};

  friend std::ostream &operator<<(std::ostream &os, const RadosSaveLogEntry &obj) {
    os << obj.op << "," << obj.pool << "," << obj.ns << "," << obj.oid << std::endl;
    return os;
  }

  friend std::istream &operator>>(std::istream &is, RadosSaveLogEntry &obj) {
    std::string line;
    std::string item;
    std::vector<std::string> csv_items;
    std::getline(is, line);

    std::stringstream line_to_parse(line);
    while (std::getline(line_to_parse, item, ',')) {
      csv_items.push_back(item);
    }

    // read obj from stream
    if (csv_items.size() == 4) {
      obj.op = csv_items[0];
      obj.pool = csv_items[1];
      obj.ns = csv_items[2];
      obj.oid = csv_items[3];
    } else {
      is.setstate(std::ios::failbit);
    }
    return is;
  }

 public:
  std::string oid;   // oid
  std::string ns;    // namespace
  std::string pool;  // storage pool
  std::string op;    // operation: save, cp (copy), mv (move)
};

class RadosSaveLog {
 public:
  explicit RadosSaveLog(const std::string &logfile_) : logfile(logfile_) {
    log_active = !logfile.empty();
  }
  RadosSaveLog() { log_active = false; }
  void set_save_log_file(const std::string &logfile_) {
    this->logfile = logfile_;
    this->log_active = !logfile.empty();
  }
  virtual ~RadosSaveLog(){};
  bool open();
  void append(const RadosSaveLogEntry &entry);
  bool close();
  bool is_open() { return ofs.is_open(); }

 private:
  std::string logfile;
  bool log_active;
  std::ofstream ofs;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_SAVE_LOG_H_ */
