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

#include <rados/librados.hpp>
#include "DictRados.hpp"

using namespace librados;
using namespace std;

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

///////////////////////////// C API //////////////////////////////

#define DICT_USERNAME_SEPARATOR '/'

static Rados cluster;
static int cluster_ref_count;

struct rados_dict {
	struct dict dict;
	DictRados *d;
};

struct rados_dict_transaction_context {
	struct dict_transaction_context ctx;
	bool atomic_inc_not_found;
	ObjectWriteOperation *write_op;
};

struct rados_dict_iterate_context {
	struct dict_iterate_context ctx;
	enum dict_iterate_flags flags;
	char *error;

	std::map<std::string, bufferlist> *readerMap;
	typename std::map<std::string, bufferlist>::iterator readerMapIter;
};

void ack_callback(rados_completion_t comp, void *arg) {
	i_debug("**** ack_callback ****");

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
	d->callback(&result, d->context);
}

void commit_callback(rados_completion_t comp, void *arg) {
	i_debug("**** commit_callback ****");
}

static const char *rados_escape_username(const char *username) {
	const char *p;
	string_t *str = t_str_new(64);

	for (p = username; *p != '\0'; p++) {
		switch (*p) {
		case DICT_USERNAME_SEPARATOR:
			str_append(str, "\\-");
			break;
		case '\\':
			str_append(str, "\\\\");
			break;
		default:
			str_append_c(str, *p);
		}
	}
	return str_c(str);
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
			i_debug("initCluster()=%d", err);
			if (err < 0) {
				*error_r = t_strdup_printf("Couldn't create the cluster handle! %s", strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.conf_parse_env(nullptr);
			i_debug("conf_parse_env()=%d", err);
			if (err < 0) {
				*error_r = t_strdup_printf("Cannot read config file: %s", strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.conf_read_file(nullptr);
			i_debug("readConfigFile()=%d", err);
			if (err < 0) {
				*error_r = t_strdup_printf("Cannot read config file: %s", strerror(-err));
				ret = -1;
			}
		}

		if (ret >= 0) {
			err = cluster.connect();
			i_debug("connect()=%d", err);
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
		err = cluster.ioctx_create(dict->d->pool.c_str(), dict->d->io_ctx);
		i_debug("createIOContext(pool=%s)=%d", dict->d->pool.c_str(), err);
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

	if (strchr(set->username, DICT_USERNAME_SEPARATOR) == NULL) {
		dict->d->username = set->username;
	} else {
		/* escape the username */
		dict->d->username = rados_escape_username(set->username);
	}

	dict->d->io_ctx.set_namespace(dict->d->username);
	i_debug("setIOContextNamespace(%s)", dict->d->username.c_str());

	*dict_r = &dict->dict;
	return 0;
}

void rados_dict_deinit(struct dict *_dict) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	DictRados *d = ((struct rados_dict *) _dict)->d;

	i_debug("rados_dict_deinit(), cluster_ref_count=%d", cluster_ref_count);

	d->io_ctx.close();
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

	ObjectReadOperation oro;
	set<string> keys;
	keys.insert(key);
	map<std::string, bufferlist> map;
	oro.omap_get_vals_by_keys(keys, &map, &r_val);
	bufferlist bl;
	oro.set_op_flags2(OPERATION_NOFLAG);

	int err = d->io_ctx.operate(d->oid, &oro, &bl);

	i_debug("rados_read_op_operate(namespace=%s,oid=%s)=%d(%s),%d(%s)", d->username.c_str(), d->oid.c_str(), err, strerror(-err),
			r_val, strerror(-r_val));

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

	/*
	 rados_write_op_t rop = rados_create_read_op();
	 rados_read_op_omap_get_vals_by_keys(rop, &key, 1, &iter, &r_val);

	 rados_completion_t completion;
	 rados_aio_create_completion(NULL, NULL, NULL, &completion);

	 int err = -1; // rados_aio_read_op_operate(rop, dict->io, completion, dict->oid, LIBRADOS_OPERATION_NOFLAG);

	 rados_aio_wait_for_complete(completion);
	 rados_aio_get_return_value(completion);
	 rados_aio_release(completion);
	 */

	//ObjectReadOperation *poro = new ObjectReadOperation;
	set<string> keys;
	keys.insert(key);
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

	AioCompletion* completion = cluster.aio_create_completion(d, ack_callback, commit_callback);

	i_debug("rados_dict_lookup_async_3(%s)", key);

	int fl = 0;
	int err = d->io_ctx.aio_operate(d->oid, completion, &d->readOperation, LIBRADOS_OPERATION_NOFLAG, &d->bufferList);

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

struct dict_transaction_context *rados_transaction_init(struct dict *_dict) {
	struct rados_dict_transaction_context *ctx;

	ctx = i_new(struct rados_dict_transaction_context, 1);
	ctx->ctx.dict = _dict;
	ctx->write_op = new ObjectWriteOperation();

	return &ctx->ctx;
}

int rados_transaction_commit(struct dict_transaction_context *_ctx, bool async, dict_transaction_commit_callback_t *callback,
		void *context) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	DictRados *d = ((struct rados_dict *) _ctx->dict)->d;

	int ret = DICT_COMMIT_RET_OK;

	if (ctx->write_op && _ctx->changed) {
		ctx->write_op->set_op_flags2(OPERATION_NOFLAG);
		int err = d->io_ctx.operate(d->oid, ctx->write_op);

		delete ctx->write_op;
		ctx->write_op = nullptr;

		if (err < 0)
			ret = DICT_COMMIT_RET_FAILED;
		else if (ctx->atomic_inc_not_found)
			ret = DICT_COMMIT_RET_NOTFOUND; // TODO DICT_COMMIT_RET_NOTFOUND = dict_atomic_inc() was used on a nonexistent key
		else
			ret = DICT_COMMIT_RET_OK;
	}

	if (callback != NULL)
		callback(ret, context);

	i_free(ctx);

	return ret;
}

void rados_transaction_rollback(struct dict_transaction_context *_ctx) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;

	delete ctx->write_op;
	ctx->write_op = nullptr;

	i_free(ctx);
}

void rados_set(struct dict_transaction_context *_ctx, const char *key, const char *value) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	i_debug("rados_set(%s,%s)", key, value);

	if (ctx->write_op) {
		const size_t len = strlen(value) + 1;  // store complete cstr
		_ctx->changed = TRUE;

		struct rados_dict *dict = (struct rados_dict *) ctx->ctx.dict;
		std::map<std::string, bufferlist> map;
		bufferlist bl;
		bl.append(value);
		map.insert(pair<string, bufferlist>(key, bl));
		ctx->write_op->omap_set(map);
	}
}

void rados_unset(struct dict_transaction_context *_ctx, const char *key) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	i_debug("rados_unset(%s)", key);

	if (ctx->write_op) {
		_ctx->changed = TRUE;

		struct rados_dict *dict = (struct rados_dict *) ctx->ctx.dict;
		set<string> keys;
		keys.insert(key);
		ctx->write_op->omap_rm_keys(keys);
	}
}

void rados_atomic_inc(struct dict_transaction_context *_ctx, const char *key, long long diff) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	i_debug("rados_atomic_inc(%s,%lld)", key, diff);

	if (ctx->write_op) {
		// TODO implement
		_ctx->changed = TRUE;
	}
}

struct dict_iterate_context *
rados_dict_iterate_init(struct dict *_dict, const char * const *paths, enum dict_iterate_flags flags) {
	DictRados *d = ((struct rados_dict *) _dict)->d;
	int rval = -1;

	/* these flags are not supported for now */
	i_assert((flags & DICT_ITERATE_FLAG_RECURSE) == 0);
	i_assert((flags & DICT_ITERATE_FLAG_EXACT_KEY) == 0);
	i_assert((flags & DICT_ITERATE_FLAG_SORT_BY_VALUE) == 0);

	struct rados_dict_iterate_context *iter = i_new(struct rados_dict_iterate_context, 1);
	iter->ctx.dict = _dict;
	iter->flags = flags;
	iter->error = NULL;

	ObjectReadOperation read_op;

	set<string> keys;
	while (*paths) {
		keys.insert(*paths);
		paths++;
	}

	iter->readerMap = new std::map<std::string, bufferlist>();
	read_op.omap_get_vals_by_keys(keys, iter->readerMap, &rval);
	bufferlist bl;
	int err = d->io_ctx.operate(d->oid, &read_op, &bl);

	if (err == 0) {
		iter->readerMapIter = iter->readerMap->begin();
	} else {
		if (err < 0) {
			iter->error = i_strdup_printf("rados_read_op_operate() failed: %d", err);
		}

		if (rval < 0) {
			iter->error = i_strdup_printf("rados_read_op_omap_get_vals_by_keys() failed: %d", rval);
		}

		delete iter->readerMap;
		iter->readerMap = nullptr;
	}

	return &iter->ctx;
}

bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;

	*key_r = NULL;
	*value_r = NULL;

	if (iter->error != NULL)
		return FALSE;

	if (iter->readerMapIter == iter->readerMap->end()) {
		return FALSE;
	} else {
		i_debug("Iterator found key = '%s', value = '%s'", iter->readerMapIter->first.c_str(),
				iter->readerMapIter->second.to_str().c_str());
		*key_r = i_strdup(iter->readerMapIter->first.c_str());
		if ((iter->flags & DICT_ITERATE_FLAG_NO_VALUE) == 0) {
			*value_r = i_strdup(iter->readerMapIter->second.to_str().c_str());
		}
		iter->readerMapIter++;
	}

	return TRUE;
}

int rados_dict_iterate_deinit(struct dict_iterate_context *ctx) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;
	int ret = iter->error != NULL ? -1 : 0;

	delete iter->readerMap;
	iter->readerMap = nullptr;

	i_free(iter->error);
	i_free(iter);
	return ret;
}
