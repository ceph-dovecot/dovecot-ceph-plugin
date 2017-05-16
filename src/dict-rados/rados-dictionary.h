/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_DICT_RADOS_RADOS_DICTIONARY_H_
#define SRC_DICT_RADOS_RADOS_DICTIONARY_H_

#include <list>
#include <string>
#include <cstdint>

extern "C" {
#include "lib.h"
#include "dict-private.h"
}

#include <rados/librados.hpp>

typedef std::shared_ptr<librados::AioCompletion> AioCompletionPtr;

class RadosDictionary {
 public:
  RadosDictionary(librados::IoCtx* ctx, const std::string& username, const std::string& oid);
  virtual ~RadosDictionary();

  const std::string get_full_oid(const std::string& key);
  const std::string get_shared_oid();
  const std::string get_private_oid();

  const std::string& get_oid() const { return oid; }

  const std::string& get_username() const { return username; }

  librados::IoCtx& get_io_ctx() { return io_ctx; }

  std::list<AioCompletionPtr> completions;

  int get(const std::string& key, std::string* value_r);

 private:
  librados::IoCtx io_ctx;
  std::string oid;
  std::string username;
};

#endif /* SRC_DICT_RADOS_RADOS_DICTIONARY_H_ */
