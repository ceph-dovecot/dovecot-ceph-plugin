/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_RADOS_DICTIONARY_H_
#define SRC_LIBRMB_RADOS_DICTIONARY_H_

#include <list>
#include <string>
#include <cstdint>
#include <mutex>

#include <rados/librados.hpp>

namespace librmb {

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

  void remove_completion(librados::AioCompletion* c);
  void push_back_completion(librados::AioCompletion* c);
  void wait_for_completions();

  int get(const std::string& key, std::string* value_r);

 private:
  librados::IoCtx io_ctx;
  std::string oid;
  std::string username;

  std::list<librados::AioCompletion*> completions;
  std::mutex completions_mutex;
};

}  // namespace librmb

#endif  // SRC_LIBRMB_RADOS_DICTIONARY_H_
