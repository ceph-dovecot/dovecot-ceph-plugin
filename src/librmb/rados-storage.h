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
  RadosStorage(librados::IoCtx* ctx, const std::string& username);
  virtual ~RadosStorage();

  librados::IoCtx& get_io_ctx() { return io_ctx; }

  const std::string& get_username() const { return username; }
  const uint64_t get_read_buffer_size() { return read_buffer_size; }
  void set_read_buffer_size(const uint64_t size) { this->read_buffer_size = size; }

 private:
  librados::IoCtx io_ctx;
  std::string username;
  uint64_t read_buffer_size;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_STORAGE_H_
