// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_LIBRMB_INTERFACES_RADOS_DICTIONARY_INTERFACE_H_
#define SRC_LIBRMB_INTERFACES_RADOS_DICTIONARY_INTERFACE_H_

#include <string>

#include <rados/librados.hpp>

namespace librmb {

class RadosDictionary {
 public:
  virtual ~RadosDictionary() {}

  virtual const std::string get_full_oid(const std::string& key) = 0;
  virtual const std::string get_shared_oid() = 0;
  virtual const std::string get_private_oid() = 0;

  virtual const std::string& get_oid() = 0;

  virtual const std::string& get_username() = 0;

  virtual librados::IoCtx& get_io_ctx() = 0;

  virtual void remove_completion(librados::AioCompletion* c) = 0;
  virtual void push_back_completion(librados::AioCompletion* c) = 0;
  virtual void wait_for_completions() = 0;
  virtual int get(const std::string& key, std::string* value_r) = 0;
};
}  // namespace librmb

#endif  // SRC_LIBRMB_INTERFACES_RADOS_DICTIONARY_INTERFACE_H_
