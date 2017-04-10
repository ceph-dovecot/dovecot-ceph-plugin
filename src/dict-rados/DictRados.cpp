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
		pool("librmb"), callback(nullptr), context(nullptr) {
}

DictRados::~DictRados() {
}

int DictRados::read_config_from_uri(const char *uri) {
	int ret = 0;
	vector<string> props(explode(uri, ':'));
	for (vector<string>::iterator it = props.begin(); it != props.end(); ++it) {
		if (it->compare(0, 4, "oid=") == 0) {
			oid = it->substr(4);
		} else if (it->compare(0, 5, "pool=") == 0) {
			pool = it->substr(5);
		} else {
			//*error_r = t_strdup_printf("Unknown parameter: %s", *args);
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
	i_debug("set_username(%s)=%s", username.c_str(), this->username.c_str());
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

inline const string DictRados::get_shared_oid() {
	return this->oid + DICT_USERNAME_SEPARATOR + "shared";
}

inline const string DictRados::get_private_oid() {
	return this->oid + DICT_USERNAME_SEPARATOR + this->username;
}

///////////////////////////// C API //////////////////////////////

static Rados cluster;
static int cluster_ref_count;

struct rados_dict {
	struct dict dict;
	DictRados *d;
};

void complete_callback(rados_completion_t comp, void* arg) {
	i_debug("**** complete_callback ****");
	DictRados *d = (DictRados *) arg;
	map<std::string, bufferlist> *pmap = &d->readerMap;

	i_debug("**** map size = %ld", pmap->size());
	i_debug("**** key      = %s", d->lookupKey.c_str());
	string value = pmap->find(d->lookupKey)->second.to_str();
	i_debug("**** value    = %s", value.c_str());

	struct dict_lookup_result result;
	result.error = nullptr;
	result.ret = DICT_COMMIT_RET_OK;
	result.value = i_strdup(value.c_str());
	if (d->callback != nullptr) {
		d->callback(&result, d->context);
	}
}

void safe_callback(rados_completion_t comp, void *arg) {
	i_debug("**** safe_callback ****");
}

int rados_dict_init(struct dict *driver, const char *uri, const struct dict_settings *set, struct dict **dict_r,
		const char **error_r) {
	struct rados_dict *dict;
	const char * const *args;
	int ret = 0;
	int err = 0;

	i_debug("rados_dict_init(uri=%s), cluster_ref_count=%d", uri, cluster_ref_count);

	dict = i_new(struct rados_dict, 1);
	dict->d = new DictRados();
	DictRados *d = dict->d;

	ret = d->read_config_from_uri(uri);

	if (cluster_ref_count == 0) {
		if (ret >= 0) {
			err = cluster.init(nullptr);
			if (err < 0) {
				*error_r = t_strdup_printf("Couldn't create the cluster handle! %s", strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.conf_parse_env(nullptr);
			if (err < 0) {
				*error_r = t_strdup_printf("Cannot read config file: %s", strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.conf_read_file(nullptr);
			if (err < 0) {
				*error_r = t_strdup_printf("Cannot read config file: %s", strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.connect();
			if (err < 0) {
				i_debug("Cannot connect to cluster: %s", strerror(-err));
				*error_r = t_strdup_printf("Cannot connect to cluster: %s", strerror(-err));
				ret = -1;
			} else {
				cluster_ref_count++;
			}
		}
	}

	if (ret >= 0) {
		err = cluster.ioctx_create(dict->d->pool.c_str(), dict->d->private_ctx);
		dict->d->shared_ctx.dup(dict->d->private_ctx);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", dict->d->pool.c_str(), strerror(-err));
			cluster.shutdown();
			cluster_ref_count--;
			ret = -1;
		}
	}

	if (ret < 0) {
		delete dict->d;
		dict->d = nullptr;
		i_free(dict);
		return -1;
	}

	dict->dict = *driver;
	dict->d->set_username(set->username);
	dict->d->private_ctx.set_namespace(dict->d->get_username());

	*dict_r = &dict->dict;
	return 0;
}

void rados_dict_deinit(struct dict *_dict) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	DictRados *d = ((struct rados_dict *) _dict)->d;

	i_debug("rados_dict_deinit(), cluster_ref_count=%d", cluster_ref_count);

	d->private_ctx.close();
	d->shared_ctx.close();
	delete dict->d;
	dict->d = nullptr;

	i_free(_dict);

	if (cluster_ref_count > 0) {
		cluster_ref_count--;
		if (cluster_ref_count == 0) {
			cluster.shutdown();
		}
	}
}

int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r) {
	DictRados *d = ((struct rados_dict *) _dict)->d;
	int r_val = -1;
	int ret = DICT_COMMIT_RET_NOTFOUND;
	*value_r = NULL;

	i_debug("rados_dict_lookup(%s)", key);

	set<string> keys;
	keys.insert(key);

	map<std::string, bufferlist> map;
	ObjectReadOperation oro;
	oro.omap_get_vals_by_keys(keys, &map, &r_val);

	bufferlist bl;
	oro.set_op_flags2(OPERATION_NOFLAG);

	int err = d->private_ctx.operate(d->get_full_oid(key), &oro, &bl);

	i_debug("rados_read_op_operate(namespace=%s,oid=%s)=%d(%s),%d(%s)", d->get_username().c_str(), d->get_full_oid(key).c_str(),
			err, strerror(-err), r_val, strerror(-r_val));

	if (err == 0) {
		if (r_val == 0) {
			auto it = map.find(key); //map.begin();
			if (it != map.end()) {
				string val = it->second.to_str();
				i_debug("Found key = '%s', value = '%s'", it->first.c_str(), val.c_str());

				*value_r = p_strndup(pool, (const void *) val.c_str(), (size_t) val.length());
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

void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context) {
	DictRados *d = ((struct rados_dict *) _dict)->d;
	rados_omap_iter_t iter;
	int r_val = -1;
	int ret = DICT_COMMIT_RET_NOTFOUND;

	i_debug("rados_dict_lookup_async_1(%s)", key);

	set<string> keys;
	keys.insert(key);

	/*
	 pdr->setLookupKey(key);
	 pdr->setContext(context);
	 pdr->setCallback(callback);
	 pdr->clearReaderMap();
	 pdr->getReadOperation().omap_get_vals_by_keys(keys, &pdr->getReaderMap(), &r_val);

	 AioCompletion* completion = pdr->createCompletion(dict->dr, complete_callback, safe_callback);
	 pdr->clearBufferList();
	 int err = pdr->ioContextAioReadOperate(completion, &pdr->getReadOperation(), LIBRADOS_OPERATION_NOFLAG, &pdr->getBufferList());
	 i_debug("rados_dict_lookup_async_2(%d)", err);
	 */

	d->lookupKey = key;

	i_debug("Context = %s", *(char **) context);
	d->context = context;
	d->callback = callback;

	//map<std::string, bufferlist> *pmap = new map<std::string, bufferlist>();
	//poro->omap_get_vals_by_keys(keys, pmap, &r_val);

	d->readerMap.clear();
	d->readOperation.omap_get_vals_by_keys(keys, &d->readerMap, &r_val);
	//bufferlist bl;
	//bufferlist *bl = new bufferlist;

	i_debug("rados_dict_lookup_async_2(%s)", key);

	AioCompletion* completion = cluster.aio_create_completion(d, complete_callback, safe_callback);

	i_debug("rados_dict_lookup_async_3(%s)", key);

	int fl = 0;
	int err = d->private_ctx.aio_operate(d->get_full_oid(key), completion, &d->readOperation, LIBRADOS_OPERATION_NOFLAG,
			&d->bufferList);

	i_debug("rados_dict_lookup_async_4(%d)", err);

	/*
	 completion->wait_for_complete();
	 ret = completion->get_return_value();
	 completion->release();

	 i_debug("rados_aio_read_op_operate(namespace=%s,oid=%s)=%d(%s),%d(%s)", dict->d->sUsername.c_str(),
	 dict->d->sOid.c_str(), err, strerror(-err), r_val, strerror(-r_val));
	 struct dict_lookup_result result;
	 result.error = nullptr;

	 if (err < 0) {
	 result.ret = DICT_COMMIT_RET_FAILED;
	 ret = DICT_COMMIT_RET_FAILED;
	 } else {
	 if (r_val == 0) {
	 auto it = map.find(key);
	 if (it != map.end()) {
	 string val = it->second.to_str();
	 i_debug("Found key = '%s', value = '%s'", it->first.c_str(), val.c_str());

	 result.ret = DICT_COMMIT_RET_OK;
	 result.value = i_strdup(val.c_str());
	 } else {
	 result.ret = DICT_COMMIT_RET_NOTFOUND;
	 }

	 char *omap_key = NULL;
	 char *omap_val = NULL;
	 size_t omap_val_len = 0;
	 size_t err;

	 do {
	 err = rados_omap_get_next(iter, &omap_key, &omap_val, &omap_val_len);
	 if (err == 0&& !(omap_val_len == 0 && omap_key == NULL && omap_val == NULL)
	 && strcmp(key, omap_key) == 0 && omap_val != NULL) {
	 ret = DICT_COMMIT_RET_OK;
	 }
	 } while (err == 0 && !(omap_val_len == 0 && omap_key == NULL && omap_val == NULL));

	 } else {
	 result.ret = DICT_COMMIT_RET_FAILED;
	 ret = DICT_COMMIT_RET_FAILED;
	 }
	 }

	 callback(&result, context);
	 */

	/*
	 rados_release_read_op(rop);

	 return ret;
	 */
}

class rados_dict_transaction_context {
public:
	struct dict_transaction_context ctx;
	bool atomic_inc_not_found;

	ObjectWriteOperation write_op_private;
	bool dirty_private;
	bool dirty_shared;

	ObjectWriteOperation write_op_shared;

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

	return &ctx->ctx;
}

int rados_transaction_commit(struct dict_transaction_context *_ctx, bool async, dict_transaction_commit_callback_t *callback,
		void *context) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	int ret = DICT_COMMIT_RET_OK;

	if (_ctx->changed) {
		if (ret == DICT_COMMIT_RET_OK && ctx->dirty_private) {
			i_debug("rados_transaction_commit() operate(%s)", d->get_private_oid().c_str());

			ctx->write_op_private.set_op_flags2(OPERATION_NOFLAG);
			int err = d->private_ctx.operate(d->get_private_oid(), &ctx->write_op_private);

			if (err < 0)
				ret = DICT_COMMIT_RET_FAILED;
			else if (ctx->atomic_inc_not_found)
				ret = DICT_COMMIT_RET_NOTFOUND; // TODO DICT_COMMIT_RET_NOTFOUND = dict_atomic_inc() was used on a nonexistent key
			else
				ret = DICT_COMMIT_RET_OK;
		}

		if (ret == DICT_COMMIT_RET_OK && ctx->dirty_shared) {
			i_debug("rados_transaction_commit() operate(%s)", d->get_shared_oid().c_str());

			ctx->write_op_shared.set_op_flags2(OPERATION_NOFLAG);
			int err = d->private_ctx.operate(d->get_shared_oid(), &ctx->write_op_shared);
			if (err < 0)
				ret = DICT_COMMIT_RET_FAILED;
			else if (ctx->atomic_inc_not_found)
				ret = DICT_COMMIT_RET_NOTFOUND; // TODO DICT_COMMIT_RET_NOTFOUND = dict_atomic_inc() was used on a nonexistent key
			else
				ret = DICT_COMMIT_RET_OK;
		}
	}

	if (callback != NULL)
		callback(ret, context);

	delete ctx;

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

	_ctx->changed = TRUE;

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

	_ctx->changed = TRUE;

	set<string> keys;
	keys.insert(key);
	ctx->get_op(key).omap_rm_keys(keys);
}

void rados_atomic_inc(struct dict_transaction_context *_ctx, const char *key, long long diff) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	i_debug("rados_atomic_inc(%s,%lld)", key, diff);

	_ctx->changed = TRUE;

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
			int err = d->private_ctx.operate(d->get_full_oid(DICT_PATH_PRIVATE), &read_op, &bl);

			iter->failed = err < 0;
			for (auto r : iter->results) {
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
			int err = d->private_ctx.operate(d->get_full_oid(DICT_PATH_SHARED), &read_op, &bl);

			iter->failed = err < 0;
			for (auto r : iter->results) {
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
