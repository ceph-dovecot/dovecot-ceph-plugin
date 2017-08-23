/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <string>

#include <rados/librados.hpp>
#include "rados-storage.h"

using namespace librados;  // NOLINT
using namespace librmb;    // NOLINT

using std::string;

#define DICT_USERNAME_SEPARATOR '/'

RadosStorage::RadosStorage(librados::IoCtx *ctx, const int max_write_size)
    : io_ctx(*ctx), max_write_size(max_write_size) {}

RadosStorage::~RadosStorage() { get_io_ctx().close(); }

int RadosStorage::split_buffer_and_exec_op(const char *buffer, size_t buffer_length, RadosMailObject *current_object,
                                           librados::ObjectWriteOperation *write_op_xattr, uint64_t max_write) {
  size_t write_buffer_size = buffer_length;
  int ret_val = 0;
  assert(max_write > 0);

  int rest = write_buffer_size % max_write;
  int div = write_buffer_size / max_write + (rest > 0 ? 1 : 0);
  for (int i = 0; i < div; i++) {
    int offset = i * max_write;

    librados::ObjectWriteOperation *op = i == 0 ? write_op_xattr : new librados::ObjectWriteOperation();

    uint64_t length = max_write;
    if (buffer_length < ((i + 1) * length)) {
      length = rest;
    }
    const char *buf = buffer + offset;
    librados::bufferlist tmp_buffer;
    tmp_buffer.append(buf, length);
    op->write(offset, tmp_buffer);

    AioCompletion *completion = librados::Rados::aio_create_completion();
    completion->set_complete_callback(current_object, nullptr);

    (*current_object->get_completion_op_map())[completion] = op;

    ret_val = io_ctx.aio_operate(current_object->get_oid(), completion, op);
    if (ret_val < 0) {
      break;
    }
  }

  return ret_val;
}

int RadosStorage::read_mail(const std::string &oid, unsigned long &size_r, char *mail_buffer) {
  int offset = 0;
  librados::bufferlist mail_data_bl;

  std::string str_buf;
  int ret = 0;
  do {
    mail_data_bl.clear();
    ret = io_ctx.read(oid, mail_data_bl, size_r, offset);
    if (ret < 0) {
      return ret;
    }
    if (ret == 0) {
      break;
    }
    mail_data_bl.copy(0, (unsigned)ret, mail_buffer);
    offset += ret;
  } while (ret > 0);
  return ret;
}
/* extract to librmb */
