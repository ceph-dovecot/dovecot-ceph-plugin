/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_RADOS_DICTIONARY_H_
#define SRC_LIBRMB_RADOS_DICTIONARY_H_

#include <list>
#include <string>
#include <cstdint>

#include <rados/librados.hpp>

typedef std::shared_ptr<librados::AioCompletion> AioCompletionPtr;

namespace tallence {
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

  std::list<AioCompletionPtr> completions;

  int get(const std::string& key, std::string* value_r);

 private:
  librados::IoCtx io_ctx;
  std::string oid;
  std::string username;
};

}  // namespace librmb
}  // namespace tallence

#endif  // SRC_LIBRMB_RADOS_DICTIONARY_H_
