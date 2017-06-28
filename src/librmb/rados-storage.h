/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_RADOS_STORAGE_H_
#define SRC_LIBRMB_RADOS_STORAGE_H_

#include <list>
#include <string>
#include <cstdint>

#include <rados/librados.hpp>

namespace librmb {

class RadosStorage {
 public:
  RadosStorage(librados::IoCtx* ctx, const std::string& username, const int max_write_size);
  virtual ~RadosStorage();

  librados::IoCtx& get_io_ctx() { return io_ctx; }

  const std::string& get_username() const { return username; }

  const int get_max_write_size() const { return max_write_size; }
  const int get_max_write_size_bytes() const { return max_write_size * 1024 * 1024; }

 private:
  librados::IoCtx io_ctx;
  std::string username;
  int max_write_size;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_STORAGE_H_
