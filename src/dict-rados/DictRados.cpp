#include "DictRados.hpp"

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
#include "DictRados.h"

}

#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <limits.h>

#include <rados/librados.hpp>
#include "DictRados.hpp"

using namespace librados;
using namespace std;

#define DICT_USERNAME_SEPARATOR '/'

static Rados cluster;
static int cluster_ref_count;

static const vector<string> explode(const string& str, const char& sep) {
	vector<string> v;
	stringstream ss(str); // Turn the string into a stream.
	string tok;

	while (getline(ss, tok, sep)) {
		v.push_back(tok);
	}

	return v;
}

DictRados::DictRados() :
		pool("librmb") {
}

DictRados::~DictRados() {
}

int DictRados::init(const string uri, const string &username, string &error_r) {
	const char * const *args;
	int ret = 0;
	int err = 0;

	ret = read_config_from_uri(uri);

	if (cluster_ref_count == 0) {
		if (ret >= 0) {
			err = cluster.init(nullptr);
			i_debug("DictRados::init()=%d", err);
			if (err < 0) {
				error_r = "Couldn't create the cluster handle! " + string(strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.conf_parse_env(nullptr);
			i_debug("conf_parse_env()=%d", err);
			if (err < 0) {
				error_r = "Cannot parse config environment! " + string(strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.conf_read_file(nullptr);
			i_debug("conf_read_file()=%d", err);
			if (err < 0) {
				error_r = "Cannot read config file! " + string(strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.connect();
			i_debug("connect()=%d", err);
			if (err < 0) {
				error_r = "Cannot connect to cluster! " + string(strerror(-err));
				ret = -1;
			} else {
				cluster_ref_count++;
			}
		}
	}

	if (ret >= 0) {
		err = cluster.ioctx_create(pool.c_str(), private_ctx);
		i_debug("ioctx_create(pool=%s)=%d", pool.c_str(), err);
		shared_ctx.dup(private_ctx);
		if (err < 0) {
			error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", pool.c_str(), strerror(-err));
			cluster.shutdown();
			cluster_ref_count--;
			ret = -1;
		}
	}

	if (ret < 0) {
		i_debug("DictRados::init(uri=%s)=%d/%s, cluster_ref_count=%d", uri.c_str(), -1, error_r.c_str(), cluster_ref_count);
		return -1;
	}

	set_username(username);
	private_ctx.set_namespace(username);

	i_debug("DictRados::init(uri=%s)=%d, cluster_ref_count=%d", uri.c_str(), 0, cluster_ref_count);
	return 0;
}

void DictRados::deinit() {
	i_debug("DictRados::deinit(), cluster_ref_count=%d", cluster_ref_count);

	private_ctx.close();
	shared_ctx.close();

	if (cluster_ref_count > 0) {
		cluster_ref_count--;
		if (cluster_ref_count == 0) {
			cluster.shutdown();
		}
	}
}

int DictRados::read_config_from_uri(const string & uri) {
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

void DictRados::set_username(const std::string& username) {
	if (username.find( DICT_USERNAME_SEPARATOR) == string::npos) {
		this->username = username;
	} else {
		/* escape the username */
		this->username = dict_escape_string(username.c_str());
	}
}

const string DictRados::get_full_oid(const std::string& key) {
	if (key.find(DICT_PATH_SHARED) == 0) {
		return get_shared_oid();
	} else if (key.find(DICT_PATH_PRIVATE) == 0) {
		return get_private_oid();
	} else {
		i_unreached();
	}
	return "";
}

IoCtx& DictRados::get_io_ctx(const std::string& key) {
	if (key.find(DICT_PATH_SHARED) == 0) {
		return shared_ctx;
	} else if (key.find(DICT_PATH_PRIVATE) == 0) {
		return private_ctx;
	} else {
		i_unreached();
	}
}

const string DictRados::get_shared_oid() {
	return this->oid + DICT_USERNAME_SEPARATOR + "shared";
}

const string DictRados::get_private_oid() {
	return this->oid + DICT_USERNAME_SEPARATOR + this->username;
}

///////////////////////////// C API //////////////////////////////

struct rados_dict {
	struct dict dict;
	DictRados *d;
};

int rados_dict_init(struct dict *driver, const char *uri, const struct dict_settings *set, struct dict **dict_r,
		const char **error_r) {
	struct rados_dict *dict;
	const char * const *args;

	i_debug("rados_dict_init(uri=%s), cluster_ref_count=%d", uri, cluster_ref_count);

	dict = i_new(struct rados_dict, 1);
	dict->d = new DictRados();
	DictRados *d = dict->d;

	string error_msg;
	int ret = d->init(uri, set->username, error_msg);

	if (ret < 0) {
		delete dict->d;
		dict->d = nullptr;
		i_free(dict);
		*error_r = t_strdup_printf(error_msg.c_str());
		return -1;
	}

	dict->dict = *driver;
	*dict_r = &dict->dict;

	return 0;
}

void rados_dict_deinit(struct dict *_dict) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	DictRados *d = ((struct rados_dict *) _dict)->d;

	i_debug("rados_dict_deinit(), cluster_ref_count=%d", cluster_ref_count);

	d->deinit();
	delete dict->d;
	dict->d = nullptr;

	i_free(_dict);
}

int rados_dict_wait(struct dict *dict) {
	/* FIXME: we don't support this yet */
	return 0;
}

class rados_dict_lookup_context {
public:
	ObjectReadOperation read_op;
	map<string, bufferlist> map;
	int r_val = -1;
	bufferlist bufferlist;

	AioCompletion *completion;
	string key;
	string value;
	void *context = nullptr;
	dict_lookup_callback_t *callback;
};

int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r) {
	DictRados *d = ((struct rados_dict *) _dict)->d;
	int ret = DICT_COMMIT_RET_NOTFOUND;
	*value_r = NULL;

	i_debug("rados_dict_lookup(%s)", key);

	set<string> keys;
	keys.insert(key);

	rados_dict_lookup_context lc;

	lc.read_op.omap_get_vals_by_keys(keys, &lc.map, &lc.r_val);
	lc.read_op.set_op_flags2(OPERATION_NOFLAG);

	int err = d->get_io_ctx(key).operate(d->get_full_oid(key), &lc.read_op, &lc.bufferlist);

	i_debug("rados_read_op_operate(oid=%s)=%d(%s),%d(%s)", d->get_full_oid(key).c_str(), err, strerror(-err), lc.r_val,
			strerror(-lc.r_val));

	if (err == 0) {
		if (lc.r_val == 0) {
			auto it = lc.map.find(key);
			if (it != lc.map.end()) {
				lc.value = it->second.to_str();
				i_debug("rados_dict_lookup('%s')='%s'", it->first.c_str(), lc.value.c_str());

				*value_r = i_strdup(lc.value.c_str());
				ret = DICT_COMMIT_RET_OK;
			}
		} else {
			ret = DICT_COMMIT_RET_FAILED;
		}
	}

	if (err != 0) {
		if (err == -ENOENT) {
			ret = DICT_COMMIT_RET_NOTFOUND;
		} else {
			ret = DICT_COMMIT_RET_FAILED;
		}
	}

	return ret;
}

static void rados_iterate_complete_callback(rados_completion_t comp, void* arg) {
	rados_dict_lookup_context *lc = (rados_dict_lookup_context *) arg;

	struct dict_lookup_result result;
	result.error = nullptr;
	result.ret = DICT_COMMIT_RET_OK;

	i_debug("complete_callback(%s): r_val=%d(%s)", lc->key.c_str(), lc->r_val, strerror(-lc->r_val));
	if (lc->r_val == 0) {
		auto it = lc->map.find(lc->key);
		if (it != lc->map.end()) {
			lc->value = it->second.to_str();
			i_debug("rados_iterate_complete_callback('%s')='%s'", it->first.c_str(), lc->value.c_str());
			result.value = i_strdup(lc->value.c_str());
			result.ret = DICT_COMMIT_RET_OK;
		}
	} else {
		if (lc->r_val == -ENOENT) {
			result.ret = DICT_COMMIT_RET_NOTFOUND;
		} else {
			result.ret = DICT_COMMIT_RET_FAILED;
		}
	}

	if (lc->callback != nullptr) {
		i_debug("call callback func...");
		lc->callback(&result, lc->context);

		delete lc->completion;
		lc->completion = nullptr;
		delete lc;
	}
}

void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context) {
	DictRados *d = ((struct rados_dict *) _dict)->d;

	i_debug("rados_dict_lookup_async(%s)", key);

	set<string> keys;
	keys.insert(key);

	auto lc = new rados_dict_lookup_context();

	lc->key = key;
	lc->context = context;
	lc->callback = callback;
	lc->read_op.omap_get_vals_by_keys(keys, &lc->map, &lc->r_val);
	lc->read_op.set_op_flags2(OPERATION_NOFLAG);
	lc->completion = librados::Rados::aio_create_completion(lc, rados_iterate_complete_callback, nullptr);

	int err = d->get_io_ctx(key).aio_operate(d->get_full_oid(key), lc->completion, &lc->read_op, LIBRADOS_OPERATION_NOFLAG,
			&lc->bufferlist);

	if (err < 0) {
		delete lc->completion;
		lc->completion = nullptr;
		delete lc;
	}
}

class rados_dict_transaction_context {
public:
	struct dict_transaction_context ctx;
	bool atomic_inc_not_found;

	ObjectWriteOperation write_op_private;
	AioCompletion *completion_private;
	bool dirty_private;
	ObjectWriteOperation write_op_shared;
	AioCompletion *completion_shared;
	bool dirty_shared;

	void *context = nullptr;
	dict_transaction_commit_callback_t *callback;

	ObjectWriteOperation &get_op(const std::string& key) {
		if (key.find(DICT_PATH_SHARED) == 0) {
			dirty_shared |= true;
			return write_op_shared;
		} else if (key.find(DICT_PATH_PRIVATE) == 0) {
			dirty_private |= true;
			return write_op_private;
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

	ctx->write_op_private.set_op_flags2(OPERATION_NOFLAG);
	ctx->write_op_shared.set_op_flags2(OPERATION_NOFLAG);

	return &ctx->ctx;
}

void rados_dict_set_timestamp(struct dict_transaction_context *_ctx, const struct timespec *ts) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	struct timespec t = { ts->tv_sec, ts->tv_nsec };

	if (ts != NULL) {
		_ctx->timestamp.tv_sec = t.tv_sec;
		_ctx->timestamp.tv_nsec = t.tv_nsec;
		ctx->write_op_private.mtime2(&t);
		ctx->write_op_shared.mtime2(&t);
	}
}

static void rados_transaction_complete_private_callback(rados_completion_t comp, void* arg) {
	rados_dict_transaction_context *c = (rados_dict_transaction_context *) arg;

	auto ret = DICT_COMMIT_RET_OK;

	comp->
	i_debug("rados_transaction_complete_private_callback(%s): r_val=%d(%s)", c->key.c_str(), c->r_val, strerror(-c->r_val));

	if (c->callback != nullptr) {
		i_debug("call callback func...");
		c->callback(ret, c->context);

		delete c->completion_private;
		c->completion_private = nullptr;
		delete c;
	}
}

static void rados_transaction_complete_shared_callback(rados_completion_t comp, void* arg) {
	rados_dict_transaction_context *c = (rados_dict_transaction_context *) arg;

	auto ret = DICT_COMMIT_RET_OK;
}

int rados_transaction_commit(struct dict_transaction_context *_ctx, bool async, dict_transaction_commit_callback_t *callback,
		void *context) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	int ret = DICT_COMMIT_RET_OK;

	if (_ctx->changed) {
		ctx->context = context;
		ctx->callback = callback;

		if (async) {

			if (ret == DICT_COMMIT_RET_OK && ctx->dirty_private) {
				i_debug("rados_transaction_commit() async operate(%s)", d->get_private_oid().c_str());
				ctx->completion_private = librados::Rados::aio_create_completion(ctx, rados_transaction_complete_private_callback,
						nullptr);

				int err = d->get_private_ctx().aio_operate(d->get_private_oid(), ctx->completion_private, &ctx->write_op_private);
				if (err < 0)
					ret = DICT_COMMIT_RET_FAILED;
			}

			if (ret == DICT_COMMIT_RET_OK && ctx->dirty_shared) {
				i_debug("rados_transaction_commit() async operate(%s)", d->get_shared_oid().c_str());
				ctx->completion_shared = librados::Rados::aio_create_completion(ctx, rados_transaction_complete_shared_callback,
						nullptr);

				int err = d->get_shared_ctx().aio_operate(d->get_shared_oid(), ctx->completion_shared, &ctx->write_op_shared);
				if (err < 0)
					ret = DICT_COMMIT_RET_FAILED;
			}

			if (ret != DICT_COMMIT_RET_OK) {
				delete ctx->completion;
				ctx->completion = nullptr;
				delete ctx;

			}
		} else {
			if (ret == DICT_COMMIT_RET_OK && ctx->dirty_private) {
				i_debug("rados_transaction_commit() operate(%s)", d->get_private_oid().c_str());

				int err = d->get_private_ctx().operate(d->get_private_oid(), &ctx->write_op_private);

				if (err < 0)
					ret = DICT_COMMIT_RET_FAILED;
				else if (ctx->atomic_inc_not_found)
					ret = DICT_COMMIT_RET_NOTFOUND; // TODO DICT_COMMIT_RET_NOTFOUND = dict_atomic_inc() was used on a nonexistent key
				else
					ret = DICT_COMMIT_RET_OK;
			}

			if (ret == DICT_COMMIT_RET_OK && ctx->dirty_shared) {
				i_debug("rados_transaction_commit() operate(%s)", d->get_shared_oid().c_str());

				int err = d->get_shared_ctx().operate(d->get_shared_oid(), &ctx->write_op_shared);
				if (err < 0)
					ret = DICT_COMMIT_RET_FAILED;
				else if (ctx->atomic_inc_not_found)
					ret = DICT_COMMIT_RET_NOTFOUND; // TODO DICT_COMMIT_RET_NOTFOUND = dict_atomic_inc() was used on a nonexistent key
				else
					ret = DICT_COMMIT_RET_OK;
			}
			if (callback != NULL)
				callback(ret, context);
			delete ctx;
		}
	}

	return ret;
}

void rados_transaction_rollback(struct dict_transaction_context *_ctx) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;

	i_debug("rados_transaction_rollback()");

	delete ctx;
}

void rados_set(struct dict_transaction_context *_ctx, const char *key, const char *value) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	i_debug("rados_set(%s,%s)", key, value);

	std::map<std::string, bufferlist> map;
	bufferlist bl;
	bl.append(value);
	map.insert(pair<string, bufferlist>(key, bl));
	ctx->get_op(key).omap_set(map);
}

void rados_unset(struct dict_transaction_context *_ctx, const char *key) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	i_debug("rados_unset(%s)", key);

	set<string> keys;
	keys.insert(key);
	ctx->get_op(key).omap_rm_keys(keys);
}

void rados_atomic_inc(struct dict_transaction_context *_ctx, const char *key, long long diff) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	i_debug("rados_atomic_inc(%s,%lld)", key, diff);

	set<string> keys;
	keys.insert(key);

// TODO implement
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

struct dict_iterate_context *
rados_dict_iterate_init(struct dict *_dict, const char * const *paths, enum dict_iterate_flags flags) {
	DictRados *d = ((struct rados_dict *) _dict)->d;

	i_debug("rados_dict_iterate_init()");

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
		if (key.find(DICT_PATH_SHARED) == 0) {
			shared_keys.insert(key);
		} else if (key.find(DICT_PATH_PRIVATE) == 0) {
			private_keys.insert(key);
		}
	}

	if (private_keys.size() + shared_keys.size() > 0) {
		if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
			iter->results.reserve(2);
		} else {
			iter->results.reserve(private_keys.size() + shared_keys.size());
		}

		if (private_keys.size() > 0) {
			i_debug("rados_dict_iterate_init() private query");
			ObjectReadOperation read_op;

			if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
				iter->results.emplace_back();
				read_op.omap_get_vals_by_keys(private_keys, &iter->results.back().map, &iter->results.back().rval);
			} else {
				int i = 0;
				for (auto k : private_keys) {
					iter->results.emplace_back();
					iter->results.back().key = k;
					read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, nullptr, &iter->results.back().rval);
				}
			}

			bufferlist bl;
			int err = d->get_private_ctx().operate(d->get_full_oid(DICT_PATH_PRIVATE), &read_op, &bl);
			i_debug("rados_dict_iterate_init(): private err=%d(%s)", err, strerror(-err));

			iter->failed = err < 0;
			for (auto r : iter->results) {
				i_debug("rados_dict_iterate_init(): private r_val=%d(%s)", r.rval, strerror(-r.rval));
				iter->failed |= (r.rval < 0);
			}

		}

		if (!iter->failed && shared_keys.size() > 0) {
			i_debug("rados_dict_iterate_init() shared query");
			ObjectReadOperation read_op;

			if (flags & DICT_ITERATE_FLAG_EXACT_KEY) {
				iter->results.emplace_back();
				read_op.omap_get_vals_by_keys(shared_keys, &iter->results.back().map, &iter->results.back().rval);
			} else {
				int i = 0;
				for (auto k : shared_keys) {
					iter->results.emplace_back();
					iter->results.back().key = k;
					read_op.omap_get_vals2("", k, LONG_MAX, &iter->results.back().map, nullptr, &iter->results.back().rval);
				}
			}

			bufferlist bl;
			int err = d->get_shared_ctx().operate(d->get_full_oid(DICT_PATH_SHARED), &read_op, &bl);
			i_debug("rados_dict_iterate_init(): shared err=%d(%s)", err, strerror(-err));

			iter->failed = err < 0;
			for (auto r : iter->results) {
				i_debug("rados_dict_iterate_init(): shared r_val=%d(%s)", r.rval, strerror(-r.rval));
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
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;
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
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;

	int ret = iter->failed ? -1 : 0;
	i_debug("rados_dict_iterate_deinit()=%d", ret);

	delete iter;

	return ret;
}
