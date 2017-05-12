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

#include <rados/librados.hpp>

extern "C" {

#include "lib.h"
#include "array.h"
#include "fs-api.h"
#include "istream.h"
#include "str.h"
#include "dict-transaction-memory.h"
#include "dict-private.h"
#include "ostream.h"
#include "connection.h"
#include "module-dir.h"
#include "dict-rados.h"
}

#include "rados-dictionary.h"

using namespace librados;  // NOLINT

using std::string;
using std::stringstream;
using std::vector;
using std::map;
using std::pair;
using std::set;

#define DICT_USERNAME_SEPARATOR '/'

static Rados cluster;
static int cluster_ref_count;

static const vector<string> explode(const string &str, const char &sep) {
  vector<string> v;
  stringstream ss(str);  // Turn the string into a stream.
  string tok;

  while (getline(ss, tok, sep)) {
    v.push_back(tok);
  }

  return v;
}

RadosDictionary::RadosDictionary() : pool("librmb") {}

RadosDictionary::~RadosDictionary() {}

int RadosDictionary::init(const string uri, const string &username, string *error_r) {
  const char *const *args;
  int ret = 0;
  int err = 0;

  ret = read_config_from_uri(uri);

  if (cluster_ref_count == 0) {
    if (ret >= 0) {
      err = cluster.init(nullptr);
      i_debug("DictRados::init()=%d", err);
      if (err < 0) {
        *error_r = "Couldn't create the cluster handle! " + string(strerror(-err));
        ret = -1;
      }
    }

    if (ret >= 0) {
      err = cluster.conf_parse_env(nullptr);
      i_debug("conf_parse_env()=%d", err);
      if (err < 0) {
        *error_r = "Cannot parse config environment! " + string(strerror(-err));
        ret = -1;
      }
    }

    if (ret >= 0) {
      err = cluster.conf_read_file(nullptr);
      i_debug("conf_read_file()=%d", err);
      if (err < 0) {
        *error_r = "Cannot read config file! " + string(strerror(-err));
        ret = -1;
      }
    }

    if (ret >= 0) {
      err = cluster.connect();
      i_debug("connect()=%d", err);
      if (err < 0) {
        *error_r = "Cannot connect to cluster! " + string(strerror(-err));
        ret = -1;
      } else {
        cluster_ref_count++;
      }
    }
  }

  if (ret >= 0) {
    err = cluster.ioctx_create(pool.c_str(), io_ctx);
    i_debug("ioctx_create(pool=%s)=%d", pool.c_str(), err);
    if (err < 0) {
      *error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", pool.c_str(), strerror(-err));
      cluster.shutdown();
      cluster_ref_count--;
      ret = -1;
    }
  }

  if (ret < 0) {
    i_debug("DictRados::init(uri=%s)=%d/%s, cluster_ref_count=%d", uri.c_str(), -1, error_r->c_str(),
            cluster_ref_count);
    return -1;
  }

  set_username(username);

  i_debug("DictRados::init(uri=%s)=%d, cluster_ref_count=%d", uri.c_str(), 0, cluster_ref_count);
  return 0;
}

void RadosDictionary::deinit() {
  i_debug("DictRados::deinit(), cluster_ref_count=%d", cluster_ref_count);

  get_io_ctx().close();

  if (cluster_ref_count > 0) {
    cluster_ref_count--;
    if (cluster_ref_count == 0) {
      cluster.shutdown();
    }
  }
}

int RadosDictionary::read_config_from_uri(const string &uri) {
  int ret = 0;
  vector<string> props(explode(uri, ':'));
  for (vector<string>::iterator it = props.begin(); it != props.end(); ++it) {
    if (it->compare(0, 4, "oid=") == 0) {
      oid = it->substr(4);
    } else if (it->compare(0, 5, "pool=") == 0) {
      pool = it->substr(5);
    } else {
      ret = -1;
      break;
    }
  }

  return ret;
}

void RadosDictionary::set_username(const std::string &username) {
  if (username.find(DICT_USERNAME_SEPARATOR) == string::npos) {
    this->username = username;
  } else {
    /* escape the username */
    this->username = dict_escape_string(username.c_str());
  }
}

const string RadosDictionary::get_full_oid(const std::string &key) {
  if (key.find(DICT_PATH_SHARED) == 0) {
    return get_shared_oid();
  } else if (key.find(DICT_PATH_PRIVATE) == 0) {
    return get_private_oid();
  } else {
    i_unreached();
  }
  return "";
}

const string RadosDictionary::get_shared_oid() { return this->oid + DICT_USERNAME_SEPARATOR + "shared"; }

const string RadosDictionary::get_private_oid() { return this->oid + DICT_USERNAME_SEPARATOR + this->username; }
