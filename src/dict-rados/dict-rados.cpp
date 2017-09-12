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

#include <limits.h>

#include <iostream>
#include <sstream>

#include <string>

#include <iterator>
#include <map>
#include <set>
#include <vector>
#include <list>
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

#include "dict-rados.h"
}

#include "libdict-rados-plugin.h"
#include "rados-cluster.h"
#include "rados-dictionary.h"
#include "interfaces/rados-cluster-interface.h"

#ifdef NDEBUG
#define FUNC_START() ((void)0)
#define FUNC_END() ((void)0)
#define FUNC_END_RET(ignore) ((void)0)
#define FUNC_END_RET_INT(ignore) ((void)0)
#else
#define FUNC_START() i_debug("[START] %s: %s at line %d", __FILE__, __func__, __LINE__)
#define FUNC_END() i_debug("[END] %s: %s at line %d\n", __FILE__, __func__, __LINE__)
#define FUNC_END_RET(ret) i_debug("[END] %s: %s at line %d, %s\n", __FILE__, __func__, __LINE__, ret)
#define FUNC_END_RET_INT(ret) i_debug("[END] %s: %s at line %d, ret==%d\n", __FILE__, __func__, __LINE__, ret)
#endif

using std::string;
using std::stringstream;
using std::vector;
using std::list;
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

#define DICT_USERNAME_SEPARATOR '/'
static const char CACHE_DELETED[] = "_DELETED_";

struct rados_dict {
  struct dict dict;
  RadosCluster *cluster;
  RadosDictionary *d;
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

  if (uri != nullptr) {
    i_debug("rados_dict_init(uri=%s)", uri);

    vector<string> props(explode(uri, ':'));

    for (vector<string>::iterator it = props.begin(); it != props.end(); ++it) {
      if (it->compare(0, 4, "oid=") == 0) {
        oid = it->substr(4);
      } else if (it->compare(0, 5, "pool=") == 0) {
        poolname = it->substr(5);
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

  int ret = dict->cluster->init();
  if (ret < 0) {
    i_free(dict);
    *error_r = t_strdup_printf("Error initializing RadosCluster! %s", strerror(-ret));
    return -1;
  }

  dict->d = new librmb::RadosDictionaryImpl(dict->cluster, poolname, username, oid);
  dict->dict = *driver;
  *dict_r = &dict->dict;

  return 0;
}

void rados_dict_deinit(struct dict *_dict) {
  struct rados_dict *dict = (struct rados_dict *)_dict;

  // wait for open operations
  rados_dict_wait(_dict);

  delete dict->d;
  dict->d = nullptr;

  dict->cluster->deinit();
  delete dict->cluster;
  dict->cluster = nullptr;

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
        i_debug("rados_dict_lookup_complete_callback('%s')='%s'", it->first.c_str(), lc->value.c_str());
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

    i_debug("rados_dict_lookup_complete_callback(%s) call callback result=%d", lc->key.c_str(), result.ret);
    lc->callback(&result, lc->context);
  }

  delete lc;
}

void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context) {
  RadosDictionary *d = ((struct rados_dict *)_dict)->d;
  set<string> keys;
  keys.insert(key);
  auto lc = new rados_dict_lookup_context(d);

  i_debug("rados_dict_lookup_async(%s)", key);

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
  i_debug("rados_dict_lookup(%s), oid=%s, err=%d", key, d->get_full_oid(key).c_str(), err);

  if (err == 0) {
    auto value = result_map.find(key);
    if (value != result_map.end()) {
      *value_r = p_strdup(pool, value->second.to_str().c_str());
      i_debug("rados_dict_lookup(%s), err=%d, value_r=%s", key, err, *value_r);
      return RADOS_COMMIT_RET_OK;
    }
  } else if (err < 0 && err != -ENOENT) {
    i_error("rados_dict_lookup(%s), err=%d (%s)", key, err, strerror(-err));
    *error_r = t_strdup_printf("omap_get_vals_by_keys(%s) failed: %s", key, strerror(-err));
    return RADOS_COMMIT_RET_FAILED;
  }

  i_debug("rados_dict_lookup(%s), NOT FOUND, err=%d (%s)", key, err, strerror(-err));
  return RADOS_COMMIT_RET_NOTFOUND;
}

static void rados_dict_transaction_private_complete_callback(completion_t comp, void *arg);
static void rados_dict_transaction_shared_complete_callback(completion_t comp, void *arg);

#define ENORESULT 1000

class rados_dict_transaction_context {
 public:
  struct dict_transaction_context ctx;
  bool atomic_inc_not_found;

  guid_128_t guid;

  void *context = nullptr;
  dict_transaction_commit_callback_t *callback;

  std::map<std::string, string> cache;

  ObjectWriteOperation write_op_private;
  AioCompletion *completion_private;
  bool dirty_private;
  bool locked_private;
  int result_private;

  ObjectWriteOperation write_op_shared;
  AioCompletion *completion_shared;
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

    completion_private = nullptr;
    completion_shared = nullptr;

    guid_128_generate(guid);
  }
  ~rados_dict_transaction_context() {}

  ObjectWriteOperation &get_op(const std::string &key) {
    if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      dirty_private = true;
      return write_op_private;
    } else if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      dirty_shared = true;
      return write_op_shared;
    }
    i_unreached();
  }

  void set_locked(const std::string &key) {
    if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      locked_shared = true;
    } else if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      locked_private = true;
    }
  }

  bool is_locked(const std::string &key) {
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

static void rados_dict_transaction_private_complete_callback(completion_t comp ATTR_UNUSED, void *arg) {
  rados_dict_transaction_context *ctx = reinterpret_cast<rados_dict_transaction_context *>(arg);
  RadosDictionary *d = ((struct rados_dict *)ctx->ctx.dict)->d;
  bool finished = true;

  std::lock_guard<std::mutex> lock(transaction_lock);

  i_debug("rados_dict_transaction_private_complete_callback() result=%d (%s)", ctx->result_private,
          strerror(-ctx->result_private));
  if (ctx->dirty_shared) {
    finished = ctx->result_shared != -ENORESULT;
  }

  ctx->result_private = ctx->completion_private->get_return_value();

  if (ctx->locked_private) {
    int err = d->get_private_io_ctx().unlock(d->get_private_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
    i_debug("rados_dict_transaction_private_complete_callback(): unlock(%s) ret=%d (%s)", d->get_private_oid().c_str(),
            err, strerror(-err));
  }

  if (finished) {
    i_debug("rados_dict_transaction_private_complete_callback() finished...");
    if (ctx->callback != nullptr) {
      i_debug("rados_dict_transaction_private_complete_callback() call callback func...");
      int ret = ctx->atomic_inc_not_found ? RADOS_COMMIT_RET_NOTFOUND : (ctx->get_result(ctx->result_private) < 0 ||
                                                                                 ctx->get_result(ctx->result_shared) < 0
                                                                             ? RADOS_COMMIT_RET_FAILED
                                                                             : RADOS_COMMIT_RET_OK);
#if DOVECOT_PREREQ(2, 3)
      struct dict_commit_result result = {static_cast<dict_commit_ret>(ret), nullptr};  // TODO(p.mauritius): text?
      ctx->callback(&result, ctx->context);
#else
      ctx->callback(ret, ctx->context);
#endif
    }
    delete ctx;
  }
}

static void rados_dict_transaction_shared_complete_callback(completion_t comp ATTR_UNUSED, void *arg) {
  rados_dict_transaction_context *ctx = reinterpret_cast<rados_dict_transaction_context *>(arg);
  RadosDictionary *d = ((struct rados_dict *)ctx->ctx.dict)->d;
  bool finished = true;

  std::lock_guard<std::mutex> lock(transaction_lock);

  i_debug("rados_dict_transaction_shared_complete_callback() result=%d (%s)", ctx->result_shared,
          strerror(-ctx->result_shared));
  if (ctx->dirty_private) {
    finished = ctx->result_private != -ENORESULT;
  }

  ctx->result_shared = ctx->completion_shared->get_return_value();

  if (ctx->locked_shared) {
    int err = d->get_shared_io_ctx().unlock(d->get_shared_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
    i_debug("rados_dict_transaction_shared_complete_callback(): unlock(%s) ret=%d (%s)", d->get_shared_oid().c_str(),
            err, strerror(-err));
  }

  if (finished) {
    i_debug("rados_dict_transaction_shared_complete_callback() finished...");
    if (ctx->callback != nullptr) {
      i_debug("rados_dict_transaction_shared_complete_callback() call callback func...");
      int ret = ctx->atomic_inc_not_found ? RADOS_COMMIT_RET_NOTFOUND : (ctx->get_result(ctx->result_private) < 0 ||
                                                                                 ctx->get_result(ctx->result_shared) < 0
                                                                             ? RADOS_COMMIT_RET_FAILED
                                                                             : RADOS_COMMIT_RET_OK);
#if DOVECOT_PREREQ(2, 3)
      struct dict_commit_result result = {static_cast<dict_commit_ret>(ret), nullptr};  // TODO(p.mauritius): text?
      ctx->callback(&result, ctx->context);
#else
      ctx->callback(ret, ctx->context);
#endif
    }
    delete ctx;
  }
}

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
  struct rados_dict *dict = (struct rados_dict *)ctx->ctx.dict;
  RadosDictionary *d = dict->d;

  i_debug("rados_dict_transaction_commit(): async=%d", async);

  bool failed = false;
  int ret = RADOS_COMMIT_RET_OK;

  if (_ctx->changed) {
    ctx->context = context;
    ctx->callback = callback;

    if (ctx->dirty_private) {
      if (async) {
        ctx->completion_private =
            librados::Rados::aio_create_completion(ctx, rados_dict_transaction_private_complete_callback, nullptr);
      } else {
        ctx->completion_private = librados::Rados::aio_create_completion(ctx, nullptr, nullptr);
      }
      int err =
          d->get_private_io_ctx().aio_operate(d->get_private_oid(), ctx->completion_private, &ctx->write_op_private);
      i_debug("rados_dict_transaction_commit(): aio_operate(%s) ret=%d (%s)", d->get_private_oid().c_str(), err,
              strerror(-err));
      failed = err < 0;

      if (!failed && async) {
        d->push_back_completion(ctx->completion_private);
      }
    }

    if (ctx->dirty_shared) {
      if (async) {
        ctx->completion_shared =
            librados::Rados::aio_create_completion(ctx, rados_dict_transaction_shared_complete_callback, nullptr);
      } else {
        ctx->completion_shared = librados::Rados::aio_create_completion();
      }
      int err = d->get_shared_io_ctx().aio_operate(d->get_shared_oid(), ctx->completion_shared, &ctx->write_op_shared);
      i_debug("rados_dict_transaction_commit(): aio_operate(%s) ret=%d (%s)", d->get_shared_oid().c_str(), err,
              strerror(-err));
      failed |= err < 0;

      if (!failed && async) {
        d->push_back_completion(ctx->completion_shared);
      }
    }

    if (!failed) {
      if (!async) {
        if (ctx->dirty_private) {
          ctx->completion_private->wait_for_complete();
          failed = ctx->completion_private->get_return_value() < 0;
          ctx->completion_private->release();
        }
        if (ctx->dirty_shared) {
          ctx->completion_shared->wait_for_complete();
          failed |= ctx->completion_shared->get_return_value() < 0;
          ctx->completion_shared->release();
        }

        ret = ctx->atomic_inc_not_found ? RADOS_COMMIT_RET_NOTFOUND
                                        : (failed ? RADOS_COMMIT_RET_FAILED : RADOS_COMMIT_RET_OK);
        if (callback != nullptr) {
#if DOVECOT_PREREQ(2, 3)
          struct dict_commit_result result = {static_cast<dict_commit_ret>(ret), nullptr};  // TODO(p.mauritius): text?
          callback(&result, ctx->context);
#else
          callback(ret, ctx->context);
#endif
        }
        ret = ctx->atomic_inc_not_found ? RADOS_COMMIT_RET_NOTFOUND
                                        : (failed ? RADOS_COMMIT_RET_FAILED : RADOS_COMMIT_RET_OK);
        if (ctx->locked_private) {
          int err = d->get_private_io_ctx().unlock(d->get_private_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
          i_debug("rados_dict_transaction_commit(): unlock(%s) ret=%d (%s)", d->get_private_oid().c_str(), err,
                  strerror(-err));
        }
        if (ctx->locked_shared) {
          int err = d->get_shared_io_ctx().unlock(d->get_shared_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
          i_debug("rados_dict_transaction_commit(): unlock(%s) ret=%d (%s)", d->get_shared_oid().c_str(), err,
                  strerror(-err));
        }
        delete ctx;
      }
    }
  } else {
    // nothing has been changed
    ret = ctx->atomic_inc_not_found ? RADOS_COMMIT_RET_NOTFOUND : RADOS_COMMIT_RET_OK;

    if (ctx->callback != nullptr) {
#if DOVECOT_PREREQ(2, 3)
      struct dict_commit_result result = {static_cast<dict_commit_ret>(ret), nullptr};  // TODO(p.mauritius): text?
      callback(&result, ctx->context);
#else
      callback(ret, ctx->context);
#endif
    }
    delete ctx;
  }

#if DOVECOT_PREREQ(2, 3)
  return;
#else
  return ret;
#endif
}

void rados_dict_transaction_rollback(struct dict_transaction_context *_ctx) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  struct rados_dict *dict = (struct rados_dict *)ctx->ctx.dict;
  RadosDictionary *d = dict->d;

  if (ctx->locked_private) {
    d->get_private_io_ctx().unlock(d->get_private_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
  }
  if (ctx->locked_shared) {
    d->get_shared_io_ctx().unlock(d->get_shared_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
  }

  delete ctx;
}

void rados_dict_set(struct dict_transaction_context *_ctx, const char *_key, const char *value) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  const string key(_key);

  i_debug("rados_dict_set(%s,%s)", _key, value);

  _ctx->changed = TRUE;

  std::map<std::string, bufferlist> map;
  bufferlist bl;
  bl.append(value);
  map.insert(pair<string, bufferlist>(key, bl));
  ctx->get_op(key).omap_set(map);

  ctx->cache[key] = value;
}

void rados_dict_unset(struct dict_transaction_context *_ctx, const char *_key) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  const string key(_key);

  i_debug("rados_dict_unset(%s)", _key);

  _ctx->changed = TRUE;

  set<string> keys;
  keys.insert(key);
  ctx->get_op(key).omap_rm_keys(keys);

  ctx->cache[key] = CACHE_DELETED;
}

void rados_dict_atomic_inc(struct dict_transaction_context *_ctx, const char *_key, long long diff) {  // NOLINT
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  RadosDictionary *d = ((struct rados_dict *)_ctx->dict)->d;
  const string key(_key);
  string old_value = "0";

  i_debug("rados_atomic_inc(%s,%lld)", _key, diff);

  auto it = ctx->cache.find(key);
  if (it == ctx->cache.end()) {
    if (d->get(key, &old_value) == -ENOENT) {
      ctx->cache[key] = old_value = CACHE_DELETED;
      ctx->atomic_inc_not_found = true;
      i_debug("rados_dict_atomic_inc(%s,%lld) key not found!", _key, diff);

      return;
    } else {
      if (!ctx->is_locked(key)) {
        struct timeval tv = {30, 0};  // TODO(peter): config?
        int err = d->get_io_ctx(key).lock_exclusive(d->get_full_oid(key), "ATOMIC_INC", guid_128_to_string(ctx->guid),
                                                    "rados_atomic_inc(" + key + ")", &tv, 0);
        if (err == 0) {
          i_debug("rados_dict_atomic_inc(%s,%lld) lock acquired", _key, diff);
          ctx->set_locked(key);
        } else {
          i_error("rados_dict_atomic_inc(%s,%lld) lock not acquired err=%d", _key, diff, err);
          ctx->atomic_inc_not_found = true;

          return;
        }
      }
    }
  } else {
    ctx->cache[key] = old_value = it->second;
  }

  i_debug("rados_dict_atomic_inc(%s,%lld) old_value=%s", _key, diff, old_value.c_str());

  if (old_value.compare(CACHE_DELETED) == 0) {
    ctx->atomic_inc_not_found = true;

    return;
  }

  long long value;  // NOLINT
  if (str_to_llong(old_value.c_str(), &value) < 0)
    i_unreached();

  value += diff;
  string new_string_value = std::to_string(value);
  rados_dict_set(_ctx, _key, new_string_value.c_str());
}

class kv_map {
 public:
  int rval = -1;
  std::string key;
  std::map<std::string, bufferlist> map;
  typename std::map<std::string, bufferlist>::iterator map_iter;
};

class rados_dict_iterate_context {
 public:
  struct dict_iterate_context ctx;
  enum dict_iterate_flags flags;
  bool failed;
  pool_t result_pool;

  std::vector<kv_map> results;
  typename std::vector<kv_map>::iterator results_iter;

  guid_128_t guid;

  rados_dict_iterate_context(struct dict *dict, enum dict_iterate_flags _flags)
      : results(), results_iter(results.begin()) {
    i_zero(&this->ctx);
    ctx.dict = dict;
    flags = _flags;
    failed = FALSE;
    result_pool = pool_alloconly_create("iterate value pool", 256);
    guid_128_generate(this->guid);
  }

  void dump() {
    auto g = guid_128_to_string(guid);
    for (const auto &i : results) {
      for (const auto &j : i.map) {
        i_debug("rados_dict_iterate_context %s - %s=%s", g, j.first.c_str(), j.second.to_str().c_str());
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
    i_debug("rados_dict_iterate_init(%s)", key.c_str());

    if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      shared_keys.insert(key);
    } else if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      private_keys.insert(key);
    }
  }

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
      i_debug("rados_dict_iterate_init(): private query");
      private_read_completion = librados::Rados::aio_create_completion();

      if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
        iter->results.emplace_back();
        private_read_op.omap_get_vals_by_keys(private_keys, &iter->results.back().map, &iter->results.back().rval);
      } else {
        for (auto k : private_keys) {
          iter->results.emplace_back();
          iter->results.back().key = k;
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_OMAP_GET_VALS2
          bool more;
          private_read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, &more, &iter->results.back().rval);
#else
          private_read_op.omap_get_vals("", k, LONG_MAX, &iter->results.back().map, &iter->results.back().rval);
#endif
        }
      }

      bufferlist bl;
      int err =
          d->get_private_io_ctx().aio_operate(d->get_private_oid(), private_read_completion, &private_read_op, &bl);
      i_debug("rados_dict_iterate_init(): private err=%d(%s)", err, strerror(-err));
      iter->failed = err < 0;
    }

    if (!iter->failed && shared_keys.size() > 0) {
      i_debug("rados_dict_iterate_init(): shared query");
      shared_read_completion = librados::Rados::aio_create_completion();

      if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
        iter->results.emplace_back();
        shared_read_op.omap_get_vals_by_keys(shared_keys, &iter->results.back().map, &iter->results.back().rval);
      } else {
        for (auto k : shared_keys) {
          iter->results.emplace_back();
          iter->results.back().key = k;
#ifdef DOVECOT_CEPH_PLUGINS_HAVE_OMAP_GET_VALS2
          bool more;
          shared_read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, &more, &iter->results.back().rval);
#else
          shared_read_op.omap_get_vals("", k, LONG_MAX, &iter->results.back().map, &iter->results.back().rval);
#endif
        }
      }

      bufferlist bl;
      int err = d->get_shared_io_ctx().aio_operate(d->get_shared_oid(), shared_read_completion, &shared_read_op, &bl);
      i_debug("rados_dict_iterate_init(): shared err=%d(%s)", err, strerror(-err));
      iter->failed = err < 0;
    }

    if (!iter->failed && private_keys.size() > 0) {
      if (!private_read_completion->is_complete()) {
        int err = private_read_completion->wait_for_complete();
        i_debug("rados_dict_iterate_init(): priv wait_for_complete_and_cb() err=%d(%s)", err, strerror(-err));
        iter->failed = err < 0;
      }
      if (!iter->failed) {
        int err = private_read_completion->get_return_value();
        i_debug("rados_dict_iterate_init(): priv get_return_value() err=%d(%s)", err, strerror(-err));
        iter->failed |= err < 0;
      }
    }

    if (!iter->failed && shared_keys.size() > 0) {
      if (!shared_read_completion->is_complete()) {
        int err = shared_read_completion->wait_for_complete();
        i_debug("rados_dict_iterate_init(): shared wait_for_complete_and_cb() err=%d(%s)", err, strerror(-err));
        iter->failed = err < 0;
      }
      if (!iter->failed) {
        int err = shared_read_completion->get_return_value();
        i_debug("rados_dict_iterate_init(): shared get_return_value() err=%d(%s)", err, strerror(-err));
        iter->failed |= err < 0;
      }
    }

    if (!iter->failed) {
      for (auto r : iter->results) {
        i_debug("rados_dict_iterate_init(): r_val=%d(%s)", r.rval, strerror(-r.rval));
        iter->failed |= (r.rval < 0);
      }
    }

    if (!iter->failed) {
      iter->dump();
      iter->results_iter = iter->results.begin();
      iter->results_iter->map_iter = iter->results_iter->map.begin();
    } else {
      i_debug("rados_dict_iterate_init() failed");
    }

    if (private_read_completion != nullptr) {
      private_read_completion->release();
    }
    if (shared_read_completion != nullptr) {
      shared_read_completion->release();
    }
  } else {
    i_debug("rados_dict_iterate_init() no keys");
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

  i_debug("rados_dict_iterate() found key='%s', value='%s'", map_iter->first.c_str(),
          map_iter->second.to_str().c_str());

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

  return ret;
}
