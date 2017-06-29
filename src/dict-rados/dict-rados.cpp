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
#include "guid.h"

#include "dict-rados.h"
}

#include "rados-cluster.h"
#include "rados-dictionary.h"

using namespace librados;  // NOLINT

using std::string;
using std::stringstream;
using std::vector;
using std::map;
using std::pair;
using std::set;

#define DICT_USERNAME_SEPARATOR '/'
static const char CACHE_DELETED[] = "_DELETED_";
using namespace librmb;  // NOLINT

struct rados_dict {
  struct dict dict;
  RadosCluster cluster;
  RadosDictionary *d;
};

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
  const char *const *args;
  string oid = "";
  string pool = "mail_dictionaries";

  i_debug("rados_dict_init(uri=%s)", uri);

  vector<string> props(explode(uri, ':'));
  for (vector<string>::iterator it = props.begin(); it != props.end(); ++it) {
    if (it->compare(0, 4, "oid=") == 0) {
      oid = it->substr(4);
    } else if (it->compare(0, 5, "pool=") == 0) {
      pool = it->substr(5);
    } else {
      *error_r = t_strdup_printf("Invalid URI!");
      return -1;
    }
  }

  string username(set->username);
  if (username.find(DICT_USERNAME_SEPARATOR) != string::npos) {
    /* escape the username */
    username = dict_escape_string(username.c_str());
  }

  dict = i_new(struct rados_dict, 1);

  string error_msg;
  int ret = dict->cluster.init(&error_msg);

  if (ret < 0) {
    i_free(dict);
    *error_r = t_strdup_printf("%s", error_msg.c_str());
    return -1;
  }

  ret = dict->cluster.dictionary_create(pool, username, oid, &dict->d);

  if (ret < 0) {
    *error_r = t_strdup_printf("Error creating RadosDictionary()! %s", strerror(-ret));
    dict->cluster.deinit();
    return -1;
  }

  dict->dict = *driver;
  *dict_r = &dict->dict;

  return 0;
}

void rados_dict_deinit(struct dict *_dict) {
  struct rados_dict *dict = (struct rados_dict *)_dict;
  RadosDictionary *d = ((struct rados_dict *)_dict)->d;

  i_debug("rados_dict_deinit()");

  dict->cluster.deinit();
  delete dict->d;
  dict->d = nullptr;

  i_free(_dict);
}

int rados_dict_wait(struct dict *_dict) {
  struct rados_dict *dict = (struct rados_dict *)_dict;
  RadosDictionary *d = ((struct rados_dict *)_dict)->d;

  i_debug("rados_dict_wait(), n=%lu", d->completions.size());

  // TODO(p.mauritius): wait timeout?
  while (!d->completions.empty()) {
    auto c = d->completions.front();
    if (c.get() != nullptr)
      c->wait_for_complete_and_cb();
  }

  return 0;
}

static void rados_lookup_complete_callback(rados_completion_t comp, void *arg);

class rados_dict_lookup_context {
 public:
  RadosDictionary *dict;
  ObjectReadOperation read_op;
  map<string, bufferlist> result_map;
  int r_val = -1;
  bufferlist bl;

  AioCompletionPtr completion;
  string key;
  string value;
  void *context = nullptr;
  dict_lookup_callback_t *callback;

  explicit rados_dict_lookup_context(RadosDictionary *dict) : callback(nullptr) {
    this->dict = dict;
    completion = std::make_shared<AioCompletion>(
        *librados::Rados::aio_create_completion(this, rados_lookup_complete_callback, nullptr));
  }

  ~rados_dict_lookup_context() {
    i_debug("~rados_dict_lookup_context()");
    // completion->release();
    // completion = nullptr;
  }
};

static void rados_lookup_complete_callback(rados_completion_t comp, void *arg) {
  rados_dict_lookup_context *lc = reinterpret_cast<rados_dict_lookup_context *>(arg);

  struct dict_lookup_result result;
  result.value = nullptr;
  result.values = nullptr;
  result.error = nullptr;
  result.ret = DICT_COMMIT_RET_OK;

  const char *values[2];

  i_debug("rados_lookup_complete_callback(%s): ret=%d(%s)", lc->key.c_str(), lc->completion->get_return_value(),
          strerror(-lc->completion->get_return_value()));
  i_debug("rados_lookup_complete_callback(%s): r_val=%d(%s)", lc->key.c_str(), lc->r_val, strerror(-lc->r_val));

  lc->dict->completions.remove(lc->completion);

  int ret = lc->completion->get_return_value();

  if (ret == 0) {
    auto it = lc->result_map.find(lc->key);
    if (it != lc->result_map.end()) {
      lc->value = it->second.to_str();
      i_debug("rados_lookup_complete_callback('%s')='%s'", it->first.c_str(), lc->value.c_str());
      result.value = lc->value.c_str();
      result.values = values;
      values[0] = lc->value.c_str();
      values[1] = nullptr;
      result.ret = DICT_COMMIT_RET_OK;
    }
  } else {
    if (ret == -ENOENT) {
      result.ret = DICT_COMMIT_RET_NOTFOUND;
    } else {
      result.ret = DICT_COMMIT_RET_FAILED;
    }
  }

  if (lc->callback != nullptr) {
    i_debug("rados_lookup_complete_callback(%s) call callback result=%d", lc->key.c_str(), result.ret);
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

  int err = d->get_io_ctx().aio_operate(d->get_full_oid(key), lc->completion.get(), &lc->read_op,
                                        LIBRADOS_OPERATION_NOFLAG, &lc->bl);

  if (err < 0) {
    if (lc->callback != nullptr) {
      struct dict_lookup_result result;
      result.value = nullptr;
      result.error = nullptr;
      result.ret = DICT_COMMIT_RET_FAILED;

      i_debug("rados_dict_lookup_async() call callback func...");
      lc->callback(&result, context);
    }
    delete lc;
  } else {
    d->completions.push_back(lc->completion);
  }
}

static void rados_dict_lookup_sync_callback(const struct dict_lookup_result *result, void *ctx) {
  struct dict_lookup_result *res = (struct dict_lookup_result *)ctx;
  res->ret = result->ret;
  res->value = t_strdup(result->value);
  res->error = t_strdup(result->error);
  i_debug("rados_dict_lookup_sync_callback() ret=%d value=%s", result->ret, result->value);
}

int rados_dict_lookup(struct dict *dict, pool_t pool, const char *key, const char **value_r) {
  pool_t orig_pool = pool;
  struct dict_lookup_result res;
  int ret;

  i_debug("rados_dict_lookup(%s)", key);

  rados_dict_lookup_async(dict, key, rados_dict_lookup_sync_callback, &res);

  if ((ret = rados_dict_wait(dict)) == 0) {
    ret = res.ret;
    if (res.ret > 0) {
      *value_r = p_strdup(orig_pool, res.value);
    }
  }

  i_debug("rados_dict_lookup(%s)=%s", key, *value_r);

  return ret;
}

class rados_dict_transaction_context {
 public:
  struct dict_transaction_context ctx;
  bool atomic_inc_not_found;

  guid_128_t guid;

  ObjectWriteOperation write_op_private;
  AioCompletionPtr completion_private;
  bool dirty_private;
  bool locked_private;

  ObjectWriteOperation write_op_shared;
  AioCompletionPtr completion_shared;
  bool dirty_shared;
  bool locked_shared;

  void *context = nullptr;
  dict_transaction_commit_callback_t *callback;

  std::map<std::string, string> cache;

  rados_dict_transaction_context() {
    dirty_private = false;
    dirty_shared = false;
    locked_private = false;
    locked_shared = false;
    completion_private = std::make_shared<AioCompletion>(*librados::Rados::aio_create_completion());
    completion_shared = std::make_shared<AioCompletion>(*librados::Rados::aio_create_completion());
    callback = nullptr;
    atomic_inc_not_found = false;

    guid_128_generate(guid);
  }

  ~rados_dict_transaction_context() {
    i_debug("~rados_dict_transaction_context()");
    // completion_private->release();
    // completion_shared->release();
  }

  ObjectWriteOperation &get_op(const std::string &key) {
    if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      dirty_shared |= true;
      return write_op_shared;
    } else if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      dirty_private |= true;
      return write_op_private;
    }
    i_unreached();
  }

  void set_locked(const std::string &key) {
    if (!key.compare(0, strlen(DICT_PATH_SHARED), DICT_PATH_SHARED)) {
      locked_shared |= true;
    } else if (!key.compare(0, strlen(DICT_PATH_PRIVATE), DICT_PATH_PRIVATE)) {
      locked_private |= true;
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
};

struct dict_transaction_context *rados_transaction_init(struct dict *_dict) {
  struct rados_dict_transaction_context *ctx;

  ctx = new rados_dict_transaction_context();
  ctx->ctx.dict = _dict;

  ctx->ctx.timestamp.tv_sec = 0;
  ctx->ctx.timestamp.tv_nsec = 0;

  return &ctx->ctx;
}

void rados_dict_set_timestamp(struct dict_transaction_context *_ctx, const struct timespec *ts) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  RadosDictionary *d = ((struct rados_dict *)_ctx->dict)->d;

  struct timespec t = {ts->tv_sec, ts->tv_nsec};

  if (ts != NULL) {
    _ctx->timestamp.tv_sec = t.tv_sec;
    _ctx->timestamp.tv_nsec = t.tv_nsec;
    ctx->write_op_private.mtime2(&t);
    ctx->write_op_shared.mtime2(&t);
  }
}

static void rados_transaction_private_complete_callback(rados_completion_t comp, void *arg) {
  rados_dict_transaction_context *c = reinterpret_cast<rados_dict_transaction_context *>(arg);
  RadosDictionary *d = ((struct rados_dict *)c->ctx.dict)->d;

  i_debug("rados_transaction_private_complete_callback() %d %d", c->completion_private->is_complete_and_cb(),
          c->completion_shared->is_complete_and_cb());

  bool failed = c->completion_private->get_return_value() < 0;
  bool finished = !c->dirty_shared;

  d->completions.remove(c->completion_private);

  if (finished || c->completion_shared->is_complete_and_cb()) {
    failed |= c->completion_shared->get_return_value() < 0;
    finished = true;
  }

  if (finished) {
    if (c->callback != nullptr) {
      i_debug("rados_transaction_private_complete_callback() call callback func...");
      c->callback(
          c->atomic_inc_not_found ? DICT_COMMIT_RET_NOTFOUND : (failed ? DICT_COMMIT_RET_FAILED : DICT_COMMIT_RET_OK),
          c->context);
    }

    delete c;
  }
}
static void rados_transaction_shared_complete_callback(rados_completion_t comp, void *arg) {
  rados_dict_transaction_context *c = reinterpret_cast<rados_dict_transaction_context *>(arg);
  RadosDictionary *d = ((struct rados_dict *)c->ctx.dict)->d;

  i_debug("rados_transaction_shared_complete_callback() %d %d", c->completion_private->is_complete_and_cb(),
          c->completion_shared->is_complete_and_cb());

  bool failed = c->completion_shared->get_return_value() < 0;
  bool finished = !c->dirty_private;

  d->completions.remove(c->completion_shared);

  if (finished || c->completion_private->is_complete_and_cb()) {
    failed |= c->completion_private->get_return_value() < 0;
    finished = true;
  }

  if (finished) {
    if (c->callback != nullptr) {
      i_debug("rados_transaction_shared_complete_callback() call callback func...");
      c->callback(
          c->atomic_inc_not_found ? DICT_COMMIT_RET_NOTFOUND : (failed ? DICT_COMMIT_RET_FAILED : DICT_COMMIT_RET_OK),
          c->context);
    }

    delete c;
  }
}

int rados_transaction_commit(struct dict_transaction_context *_ctx, bool async,
                             dict_transaction_commit_callback_t *callback, void *context) {
  rados_dict_transaction_context *ctx = reinterpret_cast<rados_dict_transaction_context *>(_ctx);
  RadosDictionary *d = ((struct rados_dict *)_ctx->dict)->d;

  bool failed = false;

  if (_ctx->changed) {
    ctx->context = context;
    ctx->callback = callback;

    if (async) {
      ctx->completion_private->set_complete_callback(ctx, rados_transaction_private_complete_callback);
      ctx->completion_shared->set_complete_callback(ctx, rados_transaction_shared_complete_callback);
    }

    if (!failed && ctx->dirty_private) {
      i_debug("rados_transaction_commit() operate(%s)", d->get_private_oid().c_str());
      failed =
          d->get_io_ctx().aio_operate(d->get_private_oid(), ctx->completion_private.get(), &ctx->write_op_private) < 0;
      if (async)
        d->completions.push_back(ctx->completion_private);
    }

    if (!failed && ctx->dirty_shared) {
      i_debug("rados_transaction_commit() operate(%s)", d->get_shared_oid().c_str());
      failed =
          d->get_io_ctx().aio_operate(d->get_shared_oid(), ctx->completion_shared.get(), &ctx->write_op_shared) < 0;
      if (async)
        d->completions.push_back(ctx->completion_shared);
    }

    if (!failed && !async) {
      if (ctx->dirty_private) {
        failed |= ctx->completion_private->wait_for_complete_and_cb() < 0;
        failed |= ctx->completion_private->get_return_value() < 0;
      }
      if (ctx->dirty_shared) {
        failed |= ctx->completion_shared->wait_for_complete_and_cb() < 0;
        failed |= ctx->completion_shared->get_return_value() < 0;
      }
      if (callback != NULL)
        callback(failed ? DICT_COMMIT_RET_FAILED : DICT_COMMIT_RET_OK, context);
    }

    if (!async) {
      if (ctx->locked_private) {
        d->get_io_ctx().unlock(d->get_private_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
      }
      if (ctx->locked_shared) {
        d->get_io_ctx().unlock(d->get_shared_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
      }
      delete ctx;
    } else {
      if (ctx->locked_private) {
        d->get_io_ctx().aio_unlock(d->get_private_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid), nullptr);
      }
      if (ctx->locked_shared) {
        d->get_io_ctx().aio_unlock(d->get_shared_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid), nullptr);
      }
    }
  }

  return ctx->atomic_inc_not_found ? DICT_COMMIT_RET_NOTFOUND : (failed ? DICT_COMMIT_RET_FAILED : DICT_COMMIT_RET_OK);
}

void rados_transaction_rollback(struct dict_transaction_context *_ctx) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  RadosDictionary *d = ((struct rados_dict *)_ctx->dict)->d;

  i_debug("rados_transaction_rollback()");

  if (ctx->locked_private) {
    d->get_io_ctx().unlock(d->get_private_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
  }
  if (ctx->locked_shared) {
    d->get_io_ctx().unlock(d->get_shared_oid(), "ATOMIC_INC", guid_128_to_string(ctx->guid));
  }

  delete ctx;
}

void rados_set(struct dict_transaction_context *_ctx, const char *_key, const char *value) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  RadosDictionary *d = ((struct rados_dict *)_ctx->dict)->d;
  const string key(_key);

  i_debug("rados_set(%s,%s)", _key, value);

  _ctx->changed = TRUE;

  std::map<std::string, bufferlist> map;
  bufferlist bl;
  bl.append(value);
  map.insert(pair<string, bufferlist>(key, bl));
  ctx->get_op(key).omap_set(map);

  ctx->cache[key] = value;
}

void rados_unset(struct dict_transaction_context *_ctx, const char *_key) {
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  RadosDictionary *d = ((struct rados_dict *)_ctx->dict)->d;
  const string key(_key);

  i_debug("rados_unset(%s)", _key);

  _ctx->changed = TRUE;

  set<string> keys;
  keys.insert(key);
  ctx->get_op(key).omap_rm_keys(keys);

  ctx->cache[key] = CACHE_DELETED;
}

void rados_atomic_inc(struct dict_transaction_context *_ctx, const char *_key, long long diff) {  // NOLINT
  struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *)_ctx;
  RadosDictionary *d = ((struct rados_dict *)_ctx->dict)->d;
  const string key(_key);
  string old_value = "0";

  i_debug("rados_atomic_inc(%s,%lld)", _key, diff);

  auto it = ctx->cache.find(key);
  if (it == ctx->cache.end()) {
    if (d->get(key, &old_value) == -ENOENT) {
      ctx->cache[key] = old_value = CACHE_DELETED;
    } else {
      RadosDictionary *d = ((struct rados_dict *)ctx->ctx.dict)->d;
      if (!ctx->is_locked(key)) {
        struct timeval tv = {30, 0};  // TODO(peter): config?
        int err = d->get_io_ctx().lock_exclusive(d->get_full_oid(key), "ATOMIC_INC", guid_128_to_string(ctx->guid), "",
                                                 &tv, 0);
        if (err == 0) {
          ctx->set_locked(key);
        } else {
          i_error("rados_atomic_inc(%s,%lld) lock not acquired err=%d", _key, diff, err);
          ctx->atomic_inc_not_found = true;
          return;
        }
      }
    }
  } else {
    old_value = it->second;
  }

  i_debug("rados_atomic_inc(%s,%lld) old_value=%s", _key, diff, old_value.c_str());

  if (old_value.compare(CACHE_DELETED) == 0) {
    ctx->atomic_inc_not_found = true;
    return;
  }

  long long value;  // NOLINT
  if (str_to_llong(old_value.c_str(), &value) < 0)
    i_unreached();

  value += diff;
  string new_string_value = std::to_string(value);

  i_debug("rados_atomic_inc(%s,%lld) new_value=%s", _key, diff, new_string_value.c_str());

  rados_set(_ctx, _key, new_string_value.c_str());
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
  bool failed = FALSE;

  std::vector<kv_map> results;
  typename std::vector<kv_map>::iterator results_iter;
};

struct dict_iterate_context *rados_dict_iterate_init(struct dict *_dict, const char *const *paths,
                                                     enum dict_iterate_flags flags) {
  RadosDictionary *d = ((struct rados_dict *)_dict)->d;

  /* these flags are not supported for now */
  i_assert((flags & DICT_ITERATE_FLAG_SORT_BY_VALUE) == 0);
  i_assert((flags & DICT_ITERATE_FLAG_SORT_BY_KEY) == 0);
  i_assert((flags & DICT_ITERATE_FLAG_ASYNC) == 0);

  auto iter = new rados_dict_iterate_context();

  iter->ctx.dict = _dict;
  iter->flags = flags;

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
    AioCompletion *private_read_completion = librados::Rados::aio_create_completion();
    ObjectReadOperation private_read_op;
    AioCompletion *shared_read_completion = librados::Rados::aio_create_completion();
    ObjectReadOperation shared_read_op;

    if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
      iter->results.reserve(2);
    } else {
      iter->results.reserve(private_keys.size() + shared_keys.size());
    }

    if (private_keys.size() > 0) {
      i_debug("rados_dict_iterate_init() private query");

      if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
        iter->results.emplace_back();
        private_read_op.omap_get_vals_by_keys(private_keys, &iter->results.back().map, &iter->results.back().rval);
      } else {
        int i = 0;
        for (auto k : private_keys) {
          iter->results.emplace_back();
          iter->results.back().key = k;
          private_read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, nullptr,
                                         &iter->results.back().rval);
        }
      }

      bufferlist bl;
      int err = d->get_io_ctx().aio_operate(d->get_full_oid(DICT_PATH_PRIVATE), private_read_completion,
                                            &private_read_op, &bl);
      i_debug("rados_dict_iterate_init(): private err=%d(%s)", err, strerror(-err));
      // iter->failed = err < 0;
    }

    if (!iter->failed && shared_keys.size() > 0) {
      i_debug("rados_dict_iterate_init() shared query");

      if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
        iter->results.emplace_back();
        shared_read_op.omap_get_vals_by_keys(shared_keys, &iter->results.back().map, &iter->results.back().rval);
      } else {
        int i = 0;
        for (auto k : shared_keys) {
          iter->results.emplace_back();
          iter->results.back().key = k;
          shared_read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, nullptr,
                                        &iter->results.back().rval);
        }
      }

      bufferlist bl;
      int err =
          d->get_io_ctx().aio_operate(d->get_full_oid(DICT_PATH_SHARED), shared_read_completion, &shared_read_op, &bl);
      i_debug("rados_dict_iterate_init(): shared err=%d(%s)", err, strerror(-err));
      // iter->failed = err < 0;
    }

    if (!iter->failed && private_keys.size() > 0) {
      int err = private_read_completion->wait_for_complete_and_cb();
      i_debug("rados_dict_iterate_init(): priv wait_for_complete_and_cb() err=%d(%s)", err, strerror(-err));
      iter->failed = err < 0;
      err = private_read_completion->get_return_value();
      i_debug("rados_dict_iterate_init(): priv get_return_value() err=%d(%s)", err, strerror(-err));
      iter->failed |= err < 0;
    }

    if (!iter->failed && shared_keys.size() > 0) {
      int err = shared_read_completion->wait_for_complete_and_cb();
      i_debug("rados_dict_iterate_init(): shared wait_for_complete_and_cb() err=%d(%s)", err, strerror(-err));
      iter->failed = err < 0;
      err = shared_read_completion->get_return_value();
      i_debug("rados_dict_iterate_init(): shared get_return_value() err=%d(%s)", err, strerror(-err));
      iter->failed |= err < 0;
    }

    private_read_completion->release();
    shared_read_completion->release();

    if (!iter->failed) {
      for (auto r : iter->results) {
        i_debug("rados_dict_iterate_init(): r_val=%d(%s)", r.rval, strerror(-r.rval));
        iter->failed |= (r.rval < 0);
      }
    }

    if (!iter->failed) {
      auto ri = iter->results_iter = iter->results.begin();
      iter->results_iter->map_iter = iter->results_iter->map.begin();
    } else {
      i_debug("rados_dict_iterate_init() failed");
    }
  } else {
    i_debug("rados_dict_iterate_init() no keys");
    iter->failed = true;
  }

  return &iter->ctx;
}

bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r) {
  struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *)ctx;
  i_debug("rados_dict_iterate()");

  *key_r = NULL;
  *value_r = NULL;

  if (iter->failed)
    return FALSE;

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
      return rados_dict_iterate(ctx, key_r, value_r);
    }
  }

  i_debug("Iterator found key = '%s', value = '%s'", map_iter->first.c_str(), map_iter->second.to_str().c_str());
  *key_r = i_strdup(map_iter->first.c_str());

  if ((iter->flags & DICT_ITERATE_FLAG_NO_VALUE) == 0) {
    *value_r = i_strdup(map_iter->second.to_str().c_str());
  }

  return TRUE;
}

int rados_dict_iterate_deinit(struct dict_iterate_context *ctx) {
  struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *)ctx;

  int ret = iter->failed ? -1 : 0;
  i_debug("rados_dict_iterate_deinit()=%d", ret);

  delete iter;

  return 0;
}
