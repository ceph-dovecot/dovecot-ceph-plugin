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

namespace librmb {

class RadosSaveLogEntry {
 public:
  RadosSaveLogEntry(std::string &oid_, std::string &ns_, std::string &pool_) {
    this->oid = oid_;
    this->ns = ns_;
    this->pool = pool_;
  }
  ~RadosSaveLogEntry();

  friend std::ostream &operator<<(std::ofstream &os, const RadosSaveLogEntry &obj) {
    os << obj.oid << "," << obj.ns << "," << obj.pool << std::endl;
    return os;
  }

 private:
  std::string oid;   // oid
  std::string ns;    // namespace
  std::string pool;  // storage pool
};

class RadosSaveLog {
 public:
  RadosSaveLog(std::string &logfile_) {
    this->logfile = logfile_;
    log_active = !logfile.empty();
  }
  virtual ~RadosSaveLog(){};
  bool open();
  void append(RadosSaveLogEntry &entry);
  bool close();

 private:
  std::string logfile;
  bool log_active;
  std::ofstream ofs;
};

} /* namespace librmb */

#endif /* SRC_LIBRMB_RADOS_SAVE_LOG_H_ */
