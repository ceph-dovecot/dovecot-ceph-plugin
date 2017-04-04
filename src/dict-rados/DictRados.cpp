/*
 * DictRados.cpp
 *
 *  Created on: Jan 24, 2017
 *      Author: peter
 */

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

#include <rados/librados.h>
#include "DictRados.h"

}

#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace librados;
using namespace std;

DictRados::DictRados() {
	setPool("librmb");

}

DictRados::~DictRados() {
	i_debug("DictRados::~DictRados()");
}

int DictRados::readConfigFromUri(const char *uri) {
	int ret = 0;
	vector<string> props(explode(uri, ':'));
	for (vector<string>::iterator it = props.begin(); it != props.end(); ++it) {
		//for (string p : props) {
		if (it->compare(0, 4, "oid=") == 0) {
			setOid(it->substr(4));
		} else if (it->compare(0, 5, "pool=") == 0) {
			setPool(it->substr(5));
		} else {
			//*error_r = t_strdup_printf("Unknown parameter: %s", *args);
			ret = -1;
			break;
		}
	}

	return ret;
}

int DictRados::parseArguments(int argc, const char ** argv) {
	return cluster.conf_parse_argv(argc, argv);
}

int DictRados::connect() {
	return cluster.connect();
}

void DictRados::shutdown() {
	cluster.shutdown();
}

AioCompletion* DictRados::createCompletion() {
	return cluster.aio_create_completion();
}

AioCompletion* DictRados::createCompletion(void *cb_arg, callback_t cb_complete, callback_t cb_safe) {
	return cluster.aio_create_completion(cb_arg, cb_complete, cb_safe);
}

int DictRados::createIOContext(const char *name) {
	return cluster.ioctx_create(name, io_ctx);
}

void DictRados::ioContextClose() {
	io_ctx.close();
}

void DictRados::ioContextSetNamspace(const std::string& nspace) {
	io_ctx.set_namespace(nspace);
}

int DictRados::ioContextReadOperate(const std::string& oid, librados::ObjectReadOperation *op, librados::bufferlist *pbl) {
	return io_ctx.operate(oid, op, pbl);
}

int DictRados::ioContextReadOperate(librados::ObjectReadOperation *op, librados::bufferlist *pbl) {
	return ioContextReadOperate(sOid, op, pbl);
}

int DictRados::ioContextAioReadOperate(const std::string& oid, librados::AioCompletion* aioCompletion,
		librados::ObjectReadOperation *op, int flags, librados::bufferlist *pbl) {
	return io_ctx.aio_operate(oid, aioCompletion, op, flags, pbl);
}

int DictRados::ioContextAioReadOperate(librados::AioCompletion* aioCompletion, librados::ObjectReadOperation *op, int flags,
		librados::bufferlist *pbl) {
	return ioContextAioReadOperate(sOid, aioCompletion, op, flags, pbl);
}

int DictRados::ioContextWriteOperate(const std::string& oid, librados::ObjectWriteOperation *op) {
	return io_ctx.operate(oid, op);
}

int DictRados::ioContextWriteOperate(librados::ObjectWriteOperation *op) {
	return ioContextWriteOperate(sOid, op);
}

int DictRados::ioContextAioWriteOperate(const std::string& oid, librados::AioCompletion* aioCompletion,
		librados::ObjectWriteOperation *op, int flags) {
	return io_ctx.aio_operate(oid, aioCompletion, op, flags);
}

int DictRados::ioContextAioWriteOperate(librados::AioCompletion* aioCompletion, librados::ObjectWriteOperation *op, int flags) {
	return ioContextAioWriteOperate(sOid, aioCompletion, op, flags);
}

void DictRados::clearReaderMap() {
	readerMap.clear();
}

void DictRados::incrementReaderMapIterator() {
	readerMapIter++;
}

void DictRados::beginReaderMapIterator() {
	readerMapIter = readerMap.begin();
}

bool DictRados::isEndReaderMapIterator() {
	return (readerMapIter == readerMap.end());
}

static const vector<string> DictRados::explode(const string& str, const char& sep) {
	vector<string> v;
	stringstream ss(str); // Turn the string into a stream.
	string tok;

	while (getline(ss, tok, sep)) {
		v.push_back(tok);
	}

	return v;
}

///////////////////////////// C API //////////////////////////////

#define DICT_USERNAME_SEPARATOR '/'

struct rados_dict {
	struct dict dict;
	DictRados *dr;
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

	ObjectReadOperation *read_op;
};

void ack_callback(rados_completion_t comp, void *arg) {
	i_debug("**** ack_callback ****");
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

static int rados_dict_init(struct dict *driver, const char *uri, const struct dict_settings *set, struct dict **dict_r,
		const char **error_r) {
	struct rados_dict *dict;
	const char * const *args;
	int ret = 0;
	int err = 0;

	i_debug("rados_dict_init(uri=%s)", uri);

	dict = i_new(struct rados_dict, 1);
	dict->dr = new DictRados();

	ret = dict->dr->readConfigFromUri(uri);

	if (ret >= 0) {
		err = dict->dr->getCluster().init(nullptr);
		i_debug("initCluster()=%d", err);
		if (err < 0) {
			*error_r = t_strdup_printf("Couldn't create the cluster handle! %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = dict->dr->getCluster().conf_parse_env(nullptr);
		i_debug("conf_parse_env()=%d", err);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot read config file: %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = dict->dr->getCluster().conf_read_file(nullptr);
		i_debug("readConfigFile()=%d", err);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot read config file: %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = dict->dr->connect();
		i_debug("connect()=%d", err);
		if (err < 0) {
			i_debug("Cannot connect to cluster: %s", strerror(-err));
			*error_r = t_strdup_printf("Cannot connect to cluster: %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = dict->dr->createIOContext(dict->dr->getPool().c_str());
		i_debug("createIOContext(pool=%s)=%d", dict->dr->getPool().c_str(), err);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", dict->dr->getPool().c_str(), strerror(-err));
			dict->dr->shutdown();
			ret = -1;
		}
	}

	if (ret < 0) {
		delete dict->dr;
		i_free(dict);
		return -1;
	}

	dict->dict = *driver;

	if (strchr(set->username, DICT_USERNAME_SEPARATOR) == NULL) {
		dict->dr->setUsername(set->username);
	} else {
		/* escape the username */
		dict->dr->setUsername(rados_escape_username(set->username));
	}

	dict->dr->ioContextSetNamspace(dict->dr->getUsername());
	i_debug("setIOContextNamespace(%s)", dict->dr->getUsername().c_str());

	*dict_r = &dict->dict;
	return 0;
}

static void rados_dict_deinit(struct dict *_dict) {
	struct rados_dict *dict = (struct rados_dict *) _dict;

	i_debug("rados_dict_deinit");

	if (dict->dr != nullptr) {
		dict->dr->ioContextClose();
		dict->dr->shutdown();
		delete dict->dr;
		dict->dr = nullptr;
	}

	i_free(dict);
}

static int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
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

	int err = dict->dr->ioContextReadOperate(&oro, &bl);

	i_debug("rados_read_op_operate(namespace=%s,oid=%s)=%d(%s),%d(%s)", dict->dr->getUsername().c_str(), dict->dr->getOid().c_str(),
			err, strerror(-err), r_val, strerror(-r_val));

	if (err == 0) {
		if (r_val == 0) {

			auto it = map.find(key); //map.begin();
			if (it != map.end()) {
				string val = it->second.to_str();
				i_debug("Found key = '%s', value = '%s'", it->first.c_str(), val.c_str());

				*value_r = p_strndup(pool, val.c_str(), val.length());
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

static void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	rados_omap_iter_t iter;
	int r_val = -1;
	int ret = DICT_COMMIT_RET_NOTFOUND;

	i_debug("rados_dict_lookup(%s)", key);

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

	ObjectReadOperation oro;
	set<string> keys;
	keys.insert(key);
	map<std::string, bufferlist> map;
	oro.omap_get_vals_by_keys(keys, &map, &r_val);
	bufferlist bl;

	AioCompletion* completion = dict->dr->createCompletion(&map, ack_callback, commit_callback);

	int fl = 0;
	int err = dict->dr->ioContextAioReadOperate(completion, &oro, LIBRADOS_OPERATION_NOFLAG, &bl);
	completion->wait_for_complete();
	ret = completion->get_return_value();
	completion->release();

	i_debug("rados_aio_read_op_operate(namespace=%s,oid=%s)=%d(%s),%d(%s)", dict->dr->getUsername().c_str(),
			dict->dr->getOid().c_str(), err, strerror(-err), r_val, strerror(-r_val));
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

			/*
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
			 */
		} else {
			result.ret = DICT_COMMIT_RET_FAILED;
			ret = DICT_COMMIT_RET_FAILED;
		}
	}

	callback(&result, context);

	/*
	 rados_release_read_op(rop);

	 return ret;
	 */
}

static struct dict_transaction_context *rados_transaction_init(struct dict *_dict) {
	struct rados_dict_transaction_context *ctx;

	ctx = i_new(struct rados_dict_transaction_context, 1);
	ctx->ctx.dict = _dict;
	ctx->write_op = new ObjectWriteOperation();

	return &ctx->ctx;
}

static int rados_transaction_commit(struct dict_transaction_context *_ctx, bool async, dict_transaction_commit_callback_t *callback,
		void *context) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	struct rados_dict *dict = (struct rados_dict *) _ctx->dict;
	int ret = DICT_COMMIT_RET_OK;

	if (ctx->write_op && _ctx->changed) {
		ctx->write_op->set_op_flags2(OPERATION_NOFLAG);
		int err = dict->dr->ioContextWriteOperate(ctx->write_op);

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

static void rados_transaction_rollback(struct dict_transaction_context *_ctx) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;

	if (ctx->write_op) {
		delete ctx->write_op;
		ctx->write_op = nullptr;
	}

	i_free(ctx);
}

static void rados_set(struct dict_transaction_context *_ctx, const char *key, const char *value) {
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

static void rados_unset(struct dict_transaction_context *_ctx, const char *key) {
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

static void rados_atomic_inc(struct dict_transaction_context *_ctx, const char *key, long long diff) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	i_debug("rados_atomic_inc(%s,%lld)", key, diff);

	if (ctx->write_op) {
		string_t *str = t_str_new(strlen(key) + 64);
		str_printfa(str, "%s;%lld", key, diff);

		bufferlist inbl;
		inbl.append((const char *) str_data(str), str_len(str) + 1);
		ctx->write_op->exec("rmb", "atomic_inc", inbl);
		_ctx->changed = TRUE;
	}
}

static struct dict_iterate_context *
rados_dict_iterate_init(struct dict *_dict, const char * const *paths, enum dict_iterate_flags flags) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	struct rados_dict_iterate_context *iter;
	int rval = -1;

	/* these flags are not supported for now */
	i_assert((flags & DICT_ITERATE_FLAG_RECURSE) == 0);
	i_assert((flags & DICT_ITERATE_FLAG_EXACT_KEY) == 0);
	i_assert((flags & DICT_ITERATE_FLAG_SORT_BY_VALUE) == 0);

	iter = i_new(struct rados_dict_iterate_context, 1);
	iter->ctx.dict = _dict;
	iter->flags = flags;
	iter->error = NULL;

	iter->read_op = new ObjectReadOperation();
	set<string> keys;
	while (*paths) {
		keys.insert(*paths);
		paths++;
	}
	dict->dr->clearReaderMap();
	map<string, bufferlist> map;
	iter->read_op->omap_get_vals_by_keys(keys, &map, &rval);
	bufferlist bl;
	int err = dict->dr->ioContextReadOperate(iter->read_op, &bl);
	dict->dr->setReaderMap(map);

	if (err == 0) {
		dict->dr->beginReaderMapIterator();
	}

	if (err < 0) {
		iter->error = i_strdup_printf("rados_read_op_operate() failed: %d", err);
	}

	if (rval < 0) {
		iter->error = i_strdup_printf("rados_read_op_omap_get_vals_by_keys() failed: %d", rval);
	}

	delete iter->read_op;
	iter->read_op = nullptr;

	return &iter->ctx;
}

static bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;

	*key_r = NULL;
	*value_r = NULL;

	if (iter->error != NULL) // || iter->omap_iter == NULL)
		return FALSE;

	struct rados_dict *dict = (struct rados_dict *) ctx->dict;
	if (dict->dr->isEndReaderMapIterator()) {
		return FALSE;
	} else {
		i_debug("Iterator found key = '%s', value = '%s'", dict->dr->getReaderMapIter()->first.c_str(),
				dict->dr->getReaderMapIter()->second.to_str().c_str());
		*key_r = i_strdup(dict->dr->getReaderMapIter()->first.c_str());
		if ((iter->flags & DICT_ITERATE_FLAG_NO_VALUE) != 0) {
			*value_r = i_strdup(dict->dr->getReaderMapIter()->second.to_str().c_str());
		}
		dict->dr->incrementReaderMapIterator();
	}

	return TRUE;
}

static int rados_dict_iterate_deinit(struct dict_iterate_context *ctx) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;
	int ret = iter->error != NULL ? -1 : 0;

	i_free(iter->error);
	i_free(iter);
	return ret;
}
