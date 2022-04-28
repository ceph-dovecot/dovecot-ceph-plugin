// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifdef HAVE_CONFIG_H
#include "dovecot-ceph-plugin-config.h"
#endif

#include <limits.h>

#include <iostream>
#include <sstream>

#include <string>

#include <iterator>
#include <map>
#include <set>
#include <vector>
#include <algorithm>

#include <utility>
#include <cstdint>
#include <mutex>  // NOLINT

#include <rados/librados.hpp>

extern "C" {
#include "dovecot-dict.h"
#include "macros.h"
#include "dict.h"
#include "guid.h"
#include "mail-user.h"
#include "array.h"
#include "dict-rados.h"
}

#include "libdict-rados-plugin.h"
#include "../librmb/rados-cluster-impl.h"
#include "../librmb/rados-dictionary-impl.h"
#include "../librmb/rados-cluster.h"
#include "../librmb/rados-guid-generator.h"
#include "../librmb/rados-util.h"

#if DOVECOT_PREREQ(2, 3)
#define dict_lookup(dict, pool, key, value_r, error_r) dict_lookup(dict, pool, key, value_r, error_r)
#else
#define dict_lookup(dict, pool, key, value_r, error_r) dict_lookup(dict, pool, key, value_r)
#endif

#define typeof(x) __typeof__(x)

using std::string;
using std::stringstream;
using std::vector;
using std::map;
using std::pair;
using std::set;

using librados::ObjectReadOperation;
using librados::bufferlist;
using librados::AioCompletion;
using librados::ObjectWriteOperation;
using librados::completion_t;

using librmb::RadosCluster;
using librmb::RadosDictionary;
using librmb::RadosGuidGenerator;

#define DICT_USERNAME_SEPARATOR '/'

struct rados_dict {
  struct dict dict;
  RadosCluster *cluster;
  RadosDictionary *d;
  RadosGuidGenerator *guid_generator;
};

class DictGuidGenerator : public librmb::RadosGuidGenerator {
  void generate_guid(std::string *guid) override {
    guid_128_t namespace_guid;
    guid_128_generate(namespace_guid);
    *guid = guid_128_to_string(namespace_guid);
  }
};

enum rados_commit_ret {
  RADOS_COMMIT_RET_OK = 1,
  RADOS_COMMIT_RET_NOTFOUND = 0,
  RADOS_COMMIT_RET_FAILED = -1,
};

#ifndef i_zero
#define i_zero(p) memset(p, 0, sizeof(*(p)))
#endif

static const vector<string> explode(const string &str, const char &sep) {
  vector<string> v;
  stringstream ss(str);  // Turn the string into a stream.
  string tok;

  while (getline(ss, tok, sep)) {
    v.push_back(tok);
  }

  return v;
}

int rados_dict_init(struct dict *driver, const char *uri, const struct dict_settings *set, struct dict **dict_r,
                    const char **error_r) {
  struct rados_dict *dict;
  string oid = "";
  string poolname = "mail_dictionaries";
  string clustername = "ceph";
  string rados_username = "client.admin";
  string ceph_cfg = "rbox_cfg";

  if (uri != nullptr) {
    vector<string> props(explode(uri, ':'));

    for (vector<string>::iterator it = props.begin(); it != props.end(); ++it) {
      if (it->compare(0, 4, "oid=") == 0) {
        oid = it->substr(4);
      } else if (it->compare(0, 5, "pool=") == 0) {
        poolname = it->substr(5);
      } else if (it->compare(0, 18, "dict_cluster_name=") == 0) {
        clustername = it->substr(18);
      } else if (it->compare(0, 15, "dict_user_name=") == 0) {
        rados_username = it->substr(16);
      } else if (it->compare(0, 21, "dict_cfg_object_name=") == 0) {
        ceph_cfg = it->substr(21);
      } else {
        *error_r = t_strdup_printf("Invalid URI!");
        return -1;
      }
    }
  }

  string username(set->username);
  if (username.find(DICT_USERNAME_SEPARATOR) != string::npos) {
    /* escape user name */
    username = dict_escape_string(username.c_str());
  }

  dict = i_new(struct rados_dict, 1);
  dict->cluster = new librmb::RadosClusterImpl();
  int ret = dict->cluster->init(clustername, rados_username);
  if (ret < 0) {
    i_free(dict);
    *error_r = t_strdup_printf("Error initializing RadosCluster! %s", strerror(-ret));
    i_error("Cluster initialization failed with error %s, clustername(%s), rados_username(%s), error_code(%d)",
            *error_r, clustername.c_str(), username.c_str(), ret);
    return -1;
  }

  dict->guid_generator = new DictGuidGenerator();
  dict->d = new librmb::RadosDictionaryImpl(dict->cluster, poolname, username, oid, dict->guid_generator, ceph_cfg);
  dict->dict = *driver;
  *dict_r = &dict->dict;

  return 0;
}

void rados_dict_deinit(struct dict *_dict) {
  if (!_dict) {
    return;
  }

  struct rados_dict *dict = (struct rados_dict *)_dict;

  // wait for open operations
  rados_dict_wait(_dict);

  if (dict->d != nullptr) {
    delete dict->d;
    dict->d = nullptr;
  }
  if (dict->cluster != nullptr) {
    dict->cluster->deinit();
    delete dict->cluster;
    dict->cluster = nullptr;
  }
  if (dict->guid_generator != nullptr) {
    delete dict->guid_generator;
    dict->guid_generator = nullptr;
  }

  i_free(_dict);
}

static void rados_lookup_complete_callback(rados_completion_t comp, void *arg);

#if DOVECOT_PREREQ(2, 3)
void rados_dict_wait(struct dict *_dict)
#else
int rados_dict_wait(struct dict *_dict)
#endif
{
  struct rados_dict *dict = (struct rados_dict *)_dict;
  // JRSE: not required with remote update? = > yes due to async lookup
  dict->d->wait_for_completions();

#if DOVECOT_PREREQ(2, 3)
  return;
#else
  return 0;
#endif
}

class rados_dict_lookup_context {
 public:
  RadosDictionary *dict;
  ObjectReadOperation read_op;
  map<string, bufferlist> result_map;
  int r_val = -1;
  bufferlist bl;

  AioCompletion *completion;
  string key;
  string value;
  void *context = nullptr;
  dict_lookup_callback_t *callback;

  explicit rados_dict_lookup_context(RadosDictionary *_dict) : callback(nullptr) {
    dict = _dict;
    completion = librados::Rados::aio_create_completion(this, rados_lookup_complete_callback, nullptr);
  }

  ~rados_dict_lookup_context() {}
};

static void rados_lookup_complete_callback(rados_completion_t comp ATTR_UNUSED, void *arg) {
  rados_dict_lookup_context *lc = reinterpret_cast<rados_dict_lookup_context *>(arg);

  struct dict_lookup_result result;
  i_zero(&result);
  result.ret = RADOS_COMMIT_RET_NOTFOUND;

#ifdef DOVECOT_CEPH_PLUGINS_HAVE_DICT_LOOKUP_RESULT_VALUES
  const char *values[2];
#endif

  int ret = lc->completion->get_return_value();

  if (lc->callback != nullptr) {
    if (ret == 0) {
      auto it = lc->result_map.find(lc->key);
      if (it != lc->result_map.end()) {
        lc->value = it->second.to_str();
        result.value = lc->value.c_str();
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_DICT_LOOKUP_RESULT_VALUES
        values[0] = lc->value.c_str();
        values[1] = nullptr;
        result.values = values;
#endif
        result.ret = RADOS_COMMIT_RET_OK;
      } else {
        result.ret = RADOS_COMMIT_RET_NOTFOUND;
      }
    } else {
      if (ret == -ENOENT) {
        result.ret = RADOS_COMMIT_RET_NOTFOUND;
      } else {
        result.ret = RADOS_COMMIT_RET_FAILED;
      }
    }
    lc->callback(&result, lc->context);
  }

  delete lc;
  lc = NULL;
}

void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context) {
  RadosDictionary *d = ((struct rados_dict *)_dict)->d;
  set<string> keys;
  keys.insert(key);
  auto lc = new rados_dict_lookup_context(d);

  lc->key = key;
  lc->context = context;
  lc->callback = callback;
  lc->read_op.omap_get_vals_by_keys(keys, &lc->result_map, &lc->r_val);

  int err = d->get_io_ctx(key).aio_operate(d->get_full_oid(key), lc->completion, &lc->read_op,
                                           LIBRADOS_OPERATION_NOFLAG, &lc->bl);

  if (err < 0) {
    if (lc->callback != nullptr) {
      struct dict_lookup_result result;
      i_zero(&result);
      result.ret = RADOS_COMMIT_RET_FAILED;
      lc->callback(&result, context);
    }
    lc->completion->release();
    delete lc;
    lc = nullptr;
  } else {
    d->push_back_completion(lc->completion);
  }
}

#if DOVECOT_PREREQ(2, 3)
int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r, const char **error_r) {
#else
int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r) {
  const char *error = nullptr;
  const char **error_r = &error;
#endif

  struct rados_dict *dict = (struct rados_dict *)_dict;
  RadosDictionary *d = dict->d;
  set<string> keys;
  keys.insert(key);
  map<string, bufferlist> result_map;
  *value_r = nullptr;
  *error_r = nullptr;

  int err = d->get_io_ctx(key).omap_get_vals_by_keys(d->get_full_oid(key), keys, &result_map);
  if (err == 0) {
    auto value = result_map.find(key);
    if (value != result_map.end()) {
      *value_r = p_strdup(pool, value->second.to_str().c_str());
      return RADOS_COMMIT_RET_OK;
    }
  } else if (err < 0 && err != -ENOENT) {
    *error_r = NULL;  // t_strdup_printf("omap_get_vals_by_keys(%s) failed: %s", key, strerror(-err));
    return RADOS_COMMIT_RET_FAILED;
  }

  return RADOS_COMMIT_RET_NOTFOUND;
}

#define ENORESULT 1000

class rados_dict_transaction_context {
 public:
  struct dict_transaction_context ctx;
  bool atomic_inc_not_found;

  guid_128_t guid;
  std::string guid_to_str;
  void *context = nullptr;
  dict_transaction_commit_callback_t *callback;

  map<string, string> set_map;
  set<string> unset_set;
  map<string, int64_t> atomic_inc_map;

  bool dirty_private;
  bool locked_private;
  int result_private;

  bool dirty_shared;
  bool locked_shared;
  int result_shared;

  explicit rados_dict_transaction_context(struct dict *_dict) {
    dirty_private = false;
    dirty_shared = false;
    locked_private = false;
    locked_shared = false;
    result_private = -ENORESULT;
    result_shared = -ENORESULT;

    callback = nullptr;
    atomic_inc_not_found = false;

    ctx.dict = _dict;
    ctx.changed = 0;

#ifdef DOVECOT_CEPH_PLUGINS_HAVE_DICT_SET_TIMESTAMP
    ctx.timestamp.tv_sec = 0;
    ctx.timestamp.tv_nsec = 0;
#endif

    guid_128_generate(guid);
    guid_to_str = guid_128_to_string(guid);
  }
  ~rados_dict_transaction_context() {}
  bool is_private(const string &key) {
    if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      dirty_private = true;
      return true;
    } else if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      dirty_shared = true;
      return false;
    }
    i_unreached();
  }
  void set_locked(const string &key) {
    if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      locked_shared = true;
    } else if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      locked_private = true;
    }
  }

  bool is_locked(const string &key) {
    if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      return locked_shared;
    } else if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      return locked_private;
    }
    i_unreached();
  }

  int get_result(int result) {
    return result < 0 && result != -ENORESULT ? RADOS_COMMIT_RET_FAILED : RADOS_COMMIT_RET_OK;
  }

  void add_set_item(string key, string value) {
    set<string>::iterator it = unset_set.find(key);
    if (it != unset_set.end()) {
      unset_set.erase(it);
    }
    set_map[key] = value;
  }

  void add_unset_item(string key) { unset_set.insert(key); }

  void add_atomic_inc_item(string key, int64_t diff) {
    auto it = atomic_inc_map.find(key);
    if (it != atomic_inc_map.end()) {
      diff += it->second;
    }
    atomic_inc_map[key] = diff;
  }

  void deploy_set_map() {
    if (set_map.size() > 0) {
      struct rados_dict *dict = (struct rados_dict *)ctx.dict;
      RadosDictionary *d = dict->d;
#ifdef DEBUG
      i_debug("deploy_set_map: set_map size = %lu", set_map.size());
#endif
      for (auto it = set_map.begin(); it != set_map.end(); it++) {
        map<string, bufferlist> map;
        bufferlist bl;
        bl.append(it->second);
        const string key = it->first;
        map.insert(pair<string, bufferlist>(key, bl));

        std::string oid = is_private(key) ? d->get_private_oid() : d->get_shared_oid();
#ifdef DEBUG
        i_debug("deploy_set_map_value: %s , oid=%s", bl.to_str().c_str(), oid.c_str());
#endif
        if ((is_private(key) ? d->get_private_io_ctx() : d->get_shared_io_ctx()).omap_set(oid, map) < 0) {
          i_error("unable to set key(%s), oid(%s), is_private(%d)", key.c_str(), oid.c_str(), is_private(key));
        }
      }
      set_map.clear();
    }
  }

  void deploy_atomic_inc_map() {
    if (atomic_inc_map.size() > 0) {
      struct rados_dict *dict = (struct rados_dict *)ctx.dict;

      RadosDictionary *d = dict->d;
      string old_value = "0";
#ifdef DEBUG
      i_debug("deploy_atomic_inc_map: atomic_inc_map size = %lu", atomic_inc_map.size());
#endif
      for (auto it = atomic_inc_map.begin(); it != atomic_inc_map.end() && !atomic_inc_not_found; it++) {
        const string key = it->first;
        std::string oid = is_private(key) ? d->get_private_oid() : d->get_shared_oid();
        // it->second is a signed long int
        librmb::RadosUtils::osd_add(&(is_private(key) ? d->get_private_io_ctx() : d->get_shared_io_ctx()), oid, key,
                                    it->second);
      }
      atomic_inc_map.clear();
    }
  }

  void deploy_unset_set() {
    if (unset_set.size() > 0) {
      struct rados_dict *dict = (struct rados_dict *)ctx.dict;
      RadosDictionary *d = dict->d;
#ifdef DEBUG
      i_debug("deploy_unset_set: unset_set size = %lu", unset_set.size());
#endif
      for (auto it = unset_set.begin(); it != unset_set.end(); it++) {
        set<string> keys;
        const string key = *it;
        keys.insert(key);
        std::string oid = is_private(key) ? d->get_private_oid() : d->get_shared_oid();
#ifdef DEBUG
        i_debug("deploy_unset_map_value key: %s , oid=%s", key.c_str(), oid.c_str());
#endif
        if ((is_private(key) ? d->get_private_io_ctx() : d->get_shared_io_ctx()).omap_rm_keys(oid, keys) < 0) {
          i_error("unable to unset key(%s), oid(%s), is_private(%d)", key.c_str(), oid.c_str(), is_private(key));
        }
      }
      unset_set.clear();
    }
  }
};

static std::mutex transaction_lock;

struct dict_transaction_context *rados_dict_transaction_init(struct dict *_dict) {
  struct rados_dict_transaction_context *ctx = new rados_dict_transaction_context(_dict);
  return &ctx->ctx;
}

#ifdef DOVECOT_CEPH_PLUGINS_HAVE_DICT_SET_TIMESTAMP
void rados_dict_set_timestamp(struct dict_transaction_context *_ctx, const struct timespec *ts) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;

  struct timespec t = {ts->tv_sec, ts->tv_nsec};

  if (ts != NULL) {
    _ctx->timestamp.tv_sec = t.tv_sec;
    _ctx->timestamp.tv_nsec = t.tv_nsec;
    ctx->write_op_private.mtime2(&t);
    ctx->write_op_shared.mtime2(&t);
  }
}
#endif

void (*transaction_commit)(struct dict_transaction_context *ctx, bool async,
                           dict_transaction_commit_callback_t *callback, void *context);

#if DOVECOT_PREREQ(2, 3)
void rados_dict_transaction_commit(struct dict_transaction_context *_ctx, bool async,
                                   dict_transaction_commit_callback_t *callback, void *context)
#else
int rados_dict_transaction_commit(struct dict_transaction_context *_ctx, bool async,
                                  dict_transaction_commit_callback_t *callback, void *context)
#endif
{
  rados_dict_transaction_context *ctx = reinterpret_cast<rados_dict_transaction_context *>(_ctx);
  string old_value = "0";

  ctx->deploy_set_map();
  ctx->deploy_atomic_inc_map();
  ctx->deploy_unset_set();

  bool failed = false;
  int ret;

  ctx->context = context;
  ctx->callback = callback;

  ret =
      ctx->atomic_inc_not_found ? RADOS_COMMIT_RET_NOTFOUND : (failed ? RADOS_COMMIT_RET_FAILED : RADOS_COMMIT_RET_OK);
  if (callback != nullptr) {
#if DOVECOT_PREREQ(2, 3)
    struct dict_commit_result result = {static_cast<dict_commit_ret>(ret), nullptr};  // TODO(p.mauritius): text?
    callback(&result, ctx->context);
#else
    callback(ret, ctx->context);
#endif
  }

  delete ctx;
  ctx = NULL;

#if DOVECOT_PREREQ(2, 3)
  return;
#else
  return ret;
#endif
}

void rados_dict_transaction_rollback(struct dict_transaction_context *_ctx) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;

  // hmm, nothing to do here???... (jrse)

  delete ctx;
  ctx = NULL;
}

void rados_dict_set(struct dict_transaction_context *_ctx, const char *_key, const char *value) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  const string key(_key);
#ifdef DEBUG
  struct rados_dict *dict = (struct rados_dict *)ctx->ctx.dict;
  RadosDictionary *d = dict->d;
  i_debug("rados_dict_set(%s, %s, oid=%s)", _key, value, d->get_full_oid(key).c_str());
#endif
  _ctx->changed = TRUE;
  ctx->add_set_item(key, value);
}

void rados_dict_unset(struct dict_transaction_context *_ctx, const char *_key) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  const string key(_key);
#ifdef DEBUG
  struct rados_dict *dict = (struct rados_dict *)ctx->ctx.dict;
  RadosDictionary *d = dict->d;
  i_debug("rados_dict_unset(%s, oid=%s)", _key, d->get_full_oid(key).c_str());
#endif
  _ctx->changed = TRUE;
  ctx->add_unset_item(key);
}

void rados_dict_atomic_inc(struct dict_transaction_context *_ctx, const char *_key, long long diff) {  // NOLINT
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  const string key(_key);
#ifdef DEBUG
  i_debug("rados_atomic_inc(%s, %lld)", _key, diff);
#endif
  ctx->add_atomic_inc_item(key, diff);
}

class kv_map {
 public:
  int rval = -1;
  string key;
  std::map<string, bufferlist> map;
  typename std::map<string, bufferlist>::iterator map_iter;
};

class rados_dict_iterate_context {
 public:
  struct dict_iterate_context ctx;
  enum dict_iterate_flags flags;
  bool failed;
  pool_t result_pool;

  vector<kv_map> results;
  typename vector<kv_map>::iterator results_iter;

  guid_128_t guid;
  std::string guid_to_str;

  rados_dict_iterate_context(struct dict *dict, enum dict_iterate_flags _flags)
      : results(), results_iter(results.begin()) {
    i_zero(&this->ctx);
    ctx.dict = dict;
    flags = _flags;
    failed = false;
    result_pool = pool_alloconly_create("iterate value pool", 1024);
    guid_128_generate(this->guid);
    guid_to_str = guid_128_to_string(this->guid);
  }

  void dump() {
    auto g = guid_to_str;
    for (const auto &i : results) {
      for (const auto &j : i.map) {
        i_debug("rados_dict_iterate_context %s - %s=%s", g.c_str(), j.first.c_str(), j.second.to_str().c_str());
      }
    }
  }
};

struct dict_iterate_context *rados_dict_iterate_init(struct dict *_dict, const char *const *paths,
                                                     const enum dict_iterate_flags flags) {
  RadosDictionary *d = ((struct rados_dict *)_dict)->d;

  /* these flags are not supported for now */
  i_assert((flags & DICT_ITERATE_FLAG_SORT_BY_VALUE) == 0);
  i_assert((flags & DICT_ITERATE_FLAG_SORT_BY_KEY) == 0);
  i_assert((flags & DICT_ITERATE_FLAG_ASYNC) == 0);

  auto iter = new rados_dict_iterate_context(_dict, flags);

  set<string> private_keys;
  set<string> shared_keys;
  while (*paths) {
    string key = *paths++;
#ifdef DEBUG
    i_debug("rados_dict_iterate_init(%s)", key.c_str());
#endif
    if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      shared_keys.insert(key);
    } else if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      private_keys.insert(key);
    }
  }
  bufferlist bl_private;
  bufferlist bl_shared;
  if (private_keys.size() + shared_keys.size() > 0) {
    AioCompletion *private_read_completion = nullptr;
    ObjectReadOperation private_read_op;
    AioCompletion *shared_read_completion = nullptr;
    ObjectReadOperation shared_read_op;

    if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
      iter->results.reserve(2);
    } else {
      iter->results.reserve(private_keys.size() + shared_keys.size());
    }

    if (private_keys.size() > 0) {
#ifdef DEBUG
      i_debug("rados_dict_iterate_init(): private query");
#endif
      private_read_completion = librados::Rados::aio_create_completion();

      if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
        iter->results.emplace_back();
        private_read_op.omap_get_vals_by_keys(private_keys, &iter->results.back().map, &iter->results.back().rval);
      } else {
        for (auto k : private_keys) {
          iter->results.emplace_back();
          iter->results.back().key = k;
#ifdef DOVECOT_CEPH_PLUGIN_HAVE_OMAP_GET_VALS2
          bool more;
          private_read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, &more, &iter->results.back().rval);
#else
          private_read_op.omap_get_vals("", k, LONG_MAX, &iter->results.back().map, &iter->results.back().rval);
#endif
        }
      }

      int err = d->get_private_io_ctx().aio_operate(d->get_private_oid(), private_read_completion, &private_read_op,
                                                    &bl_private);
#ifdef DEBUG
      i_debug("rados_dict_iterate_init(): private err=%d(%s)", err, strerror(-err));
#endif
      iter->failed = err < 0;
    }

    if (!iter->failed && shared_keys.size() > 0) {
#ifdef DEBUG
      i_debug("rados_dict_iterate_init(): shared query");
#endif
      shared_read_completion = librados::Rados::aio_create_completion();

      if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
        iter->results.emplace_back();
        shared_read_op.omap_get_vals_by_keys(shared_keys, &iter->results.back().map, &iter->results.back().rval);
      } else {
        for (auto k : shared_keys) {
          iter->results.emplace_back();
          iter->results.back().key = k;
#ifdef DOVECOT_CEPH_PLUGIN_HAVE_OMAP_GET_VALS2
          bool more;
          shared_read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, &more, &iter->results.back().rval);
#else
          shared_read_op.omap_get_vals("", k, LONG_MAX, &iter->results.back().map, &iter->results.back().rval);
#endif
        }
      }

      int err =
          d->get_shared_io_ctx().aio_operate(d->get_shared_oid(), shared_read_completion, &shared_read_op, &bl_shared);
#ifdef DEBUG
      i_debug("rados_dict_iterate_init(): shared err=%d(%s)", err, strerror(-err));
#endif
      iter->failed = err < 0;
    }

    if (!iter->failed && private_keys.size() > 0) {
      if (!private_read_completion->is_complete()) {
        int err = private_read_completion->wait_for_complete_and_cb();
#ifdef DEBUG
        i_debug("rados_dict_iterate_init(): priv wait_for_complete_and_cb() err=%d(%s)", err, strerror(-err));
#endif
        iter->failed = err < 0;
      }
      if (!iter->failed) {
        int err = private_read_completion->get_return_value();
#ifdef DEBUG
        i_debug("rados_dict_iterate_init(): priv get_return_value() err=%d(%s)", err, strerror(-err));
#endif
        iter->failed |= err < 0;
      }
    }

    if (!iter->failed && shared_keys.size() > 0) {
      if (!shared_read_completion->is_complete()) {
        int err = shared_read_completion->wait_for_complete_and_cb();
#ifdef DEBUG
        i_debug("rados_dict_iterate_init(): shared wait_for_complete_and_cb() err=%d(%s)", err, strerror(-err));
#endif
        iter->failed = err < 0;
      }
      if (!iter->failed) {
        int err = shared_read_completion->get_return_value();
#ifdef DEBUG
        i_debug("rados_dict_iterate_init(): shared get_return_value() err=%d(%s)", err, strerror(-err));
#endif
        iter->failed |= err < 0;
      }
    }

    if (!iter->failed) {
      for (auto r : iter->results) {
#ifdef DEBUG
        i_debug("rados_dict_iterate_init(): r_val=%d(%s)", r.rval, strerror(-r.rval));
#endif
        iter->failed |= (r.rval < 0);
      }
    }

    if (!iter->failed) {
      iter->dump();
      iter->results_iter = iter->results.begin();
      iter->results_iter->map_iter = iter->results_iter->map.begin();
    }

    if (private_read_completion != nullptr) {
      private_read_completion->release();
      private_read_completion = nullptr;
    }
    if (shared_read_completion != nullptr) {
      shared_read_completion->release();
      shared_read_completion = nullptr;
    }
  } else {
#ifdef DEBUG
    i_debug("rados_dict_iterate_init() no keys");
#endif
    iter->failed = true;
  }

  return &iter->ctx;
}

bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r) {
  struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *)ctx;

  *key_r = NULL;
  *value_r = NULL;

  if (iter->failed) {
    return FALSE;
  }

  while (iter->results_iter->map_iter == iter->results_iter->map.end()) {
    if (++iter->results_iter == iter->results.end())
      return FALSE;
    iter->results_iter->map_iter = iter->results_iter->map.begin();
  }

  auto map_iter = iter->results_iter->map_iter++;

  if ((iter->flags & DICT_ITERATE_FLAG_RECURSE) != 0) {
    // match everything
  } else if ((iter->flags & DICT_ITERATE_FLAG_EXACT_KEY) != 0) {
    // prefiltered by query, match everything
  } else {
    if (map_iter->first.find('/', iter->results_iter->key.length()) != string::npos) {
      auto ret = rados_dict_iterate(ctx, key_r, value_r);
      return ret;
    }
  }
#ifdef DEBUG
  i_debug("rados_dict_iterate() found key='%s', value='%s'", map_iter->first.c_str(),
          map_iter->second.to_str().c_str());
#endif
  p_clear(iter->result_pool);

  *key_r = p_strdup(iter->result_pool, map_iter->first.c_str());

  if ((iter->flags & DICT_ITERATE_FLAG_NO_VALUE) == 0) {
    *value_r = p_strdup(iter->result_pool, map_iter->second.to_str().c_str());
  }

  return TRUE;
}

#if DOVECOT_PREREQ(2, 3)
int rados_dict_iterate_deinit(struct dict_iterate_context *ctx, const char **error_r)
#else
int rados_dict_iterate_deinit(struct dict_iterate_context *ctx)
#endif
{
  struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *)ctx;

  int ret = iter->failed ? -1 : 0;
  pool_unref(&iter->result_pool);
  delete iter;
  iter = NULL;

  return ret;
}
