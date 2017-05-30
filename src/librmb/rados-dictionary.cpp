/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <limits.h>

#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <utility>
#include <cstdint>

#include <rados/librados.hpp>

#include "rados-dictionary.h"

using namespace librados;  // NOLINT
using namespace tallence::librmb;

using std::string;
using std::stringstream;
using std::vector;
using std::map;
using std::pair;
using std::set;

#define DICT_USERNAME_SEPARATOR '/'
#define DICT_PATH_PRIVATE "priv/"
#define DICT_PATH_SHARED "shared/"

RadosDictionary::RadosDictionary(librados::IoCtx *ctx, const string &username, const string &oid)
    : io_ctx(*ctx), username(username), oid(oid) {}

RadosDictionary::~RadosDictionary() { get_io_ctx().close(); }

const string RadosDictionary::get_full_oid(const std::string &key) {
  if (key.find(DICT_PATH_SHARED) == 0) {
    return get_shared_oid();
  } else if (key.find(DICT_PATH_PRIVATE) == 0) {
    return get_private_oid();
  } else {
    // TODO(peter) i_unreached();
  }
  return "";
}

const string RadosDictionary::get_shared_oid() { return this->oid + DICT_USERNAME_SEPARATOR + "shared"; }

const string RadosDictionary::get_private_oid() { return this->oid + DICT_USERNAME_SEPARATOR + this->username; }

int RadosDictionary::get(const string &key, string *value_r) {
  int r_val = -1;

  set<string> keys;
  keys.insert(key);

  map<std::string, bufferlist> map;
  ObjectReadOperation oro;
  oro.omap_get_vals_by_keys(keys, &map, &r_val);

  bufferlist bl;
  int err = io_ctx.operate(get_full_oid(key), &oro, &bl);

  if (err == 0) {
    if (r_val == 0) {
      auto it = map.find(key);  // map.begin();
      if (it != map.end()) {
        *value_r = it->second.to_str();
        return 0;
      }
      return -ENOENT;
    } else {
      err = r_val;
    }
  }

  return err;
}
