/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_RADOS_STORAGE_H_
#define SRC_STORAGE_RBOX_RADOS_STORAGE_H_

#include <list>
#include <string>
#include <cstdint>

extern "C" {
#include "lib.h"
}

#include <rados/librados.hpp>

class RadosStorage {
 public:
  RadosStorage(librados::IoCtx* ctx, const std::string& username, const std::string& oid);
  virtual ~RadosStorage();

  const std::string& get_oid() const { return oid; }

  const std::string& get_username() const { return username; }

  librados::IoCtx& get_io_ctx() { return io_ctx; }

 private:
  librados::IoCtx io_ctx;
  std::string oid;
  std::string username;

};

#endif /* SRC_STORAGE_RBOX_RADOS_STORAGE_H_ */
