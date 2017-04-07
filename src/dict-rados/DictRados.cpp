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
	if (completion != nullptr) {
		delete completion;
	}
}

int DictRados::init(const char* uri, const char** error_r) {

	int ret = readConfigFromUri(uri);
	int err = -1;

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
		err = connect();
		i_debug("connect()=%d", err);
		if (err < 0) {
			i_debug("Cannot connect to cluster: %s", strerror(-err));
			*error_r = t_strdup_printf("Cannot connect to cluster: %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = createIOContext(sPool.c_str());
		i_debug("createIOContext(pool=%s)=%d", sPool.c_str(), err);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", sPool.c_str(), strerror(-err));
			cluster.shutdown();
			ret = -1;
		}
	}

	return ret;
}

void DictRados::deinit() {
	io_ctx.close();
	cluster.shutdown();
}

int DictRados::readConfigFromUri(const char* uri) {
	int ret = 0;
	vector<string> properties(explode(uri, ':'));
	for (auto property = properties.begin(); property != properties.end(); ++property) {
		if (property->compare(0, 4, "oid=") == 0) {
			setOid(property->substr(4));
		} else if (property->compare(0, 5, "pool=") == 0) {
			setPool(property->substr(5));
		} else {
			ret = -1;
			break;
		}
	}

	return ret;
}

int DictRados::parseArguments(int argc, const char** argv) {
	return cluster.conf_parse_argv(argc, argv);
}

int DictRados::connect() {
	return cluster.connect();
}

AioCompletion* DictRados::createCompletion() {
	return cluster.aio_create_completion();
}

AioCompletion* DictRados::createCompletion(void* cb_arg, callback_t cb_complete, callback_t cb_safe) {
	if (completion != nullptr) {
		delete completion;
		completion = nullptr;
	}
	completion = cluster.aio_create_completion(cb_arg, cb_complete, cb_safe);
	return completion;
}

int DictRados::createIOContext(const char* name) {
	return cluster.ioctx_create(name, io_ctx);
}

void DictRados::ioContextSetNamspace(const std::string& nspace) {
	io_ctx.set_namespace(nspace);
}

int DictRados::ioContextReadOperate(const std::string& oid, librados::ObjectReadOperation* op, librados::bufferlist* pbl) {
	return io_ctx.operate(oid, op, pbl);
}

int DictRados::ioContextReadOperate(librados::ObjectReadOperation* op, librados::bufferlist* pbl) {
	return ioContextReadOperate(sOid, op, pbl);
}

int DictRados::ioContextAioReadOperate(const std::string& oid, librados::AioCompletion* aioCompletion,
		librados::ObjectReadOperation* op, int flags, librados::bufferlist* pbl) {
	return io_ctx.aio_operate(oid, aioCompletion, op, flags, pbl);
}

int DictRados::ioContextAioReadOperate(librados::AioCompletion* aioCompletion, librados::ObjectReadOperation* op, int flags,
		librados::bufferlist* pbl) {
	return ioContextAioReadOperate(sOid, aioCompletion, op, flags, pbl);
}

int DictRados::ioContextWriteOperate(const std::string& oid, librados::ObjectWriteOperation* op) {
	return io_ctx.operate(oid, op);
}

int DictRados::ioContextWriteOperate(librados::ObjectWriteOperation* op) {
	return ioContextWriteOperate(sOid, op);
}

int DictRados::ioContextAioWriteOperate(const std::string& oid, librados::AioCompletion* aioCompletion,
		librados::ObjectWriteOperation* op, int flags) {
	return io_ctx.aio_operate(oid, aioCompletion, op, flags);
}

int DictRados::ioContextAioWriteOperate(librados::AioCompletion* aioCompletion, librados::ObjectWriteOperation* op, int flags) {
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

int DictRados::waitForCompletion() {
	int ret = 0;
	if (completion != nullptr) {
		ret = completion->wait_for_complete();
	}
	return ret;
}

void DictRados::clearBufferList() {
	bufferList.clear();
}

const vector<string> DictRados::explode(const string& str, const char& sep) {
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
	DictRados* dr;
};

struct rados_dict_transaction_context {
	struct dict_transaction_context ctx;
	bool atomic_inc_not_found;
	ObjectWriteOperation* write_op;
};

struct rados_dict_iterate_context {
	struct dict_iterate_context ctx;
	enum dict_iterate_flags flags;
	char* error;
	ObjectReadOperation* read_op;
};

void complete_callback(rados_completion_t comp, void* arg) {
	i_debug("**** complete_callback ****");

	DictRados* dr = (DictRados *) arg;
	map<std::string, bufferlist>* pmap = &dr->getReaderMap();

	i_debug("**** map size = %zu", pmap->size());
	string value;
	if (pmap->find(dr->getLookupKey()) != pmap->end()) {
		value = pmap->find(dr->getLookupKey())->second.to_str();
		i_debug("**** value    = %s", value.c_str());
	}

	struct dict_lookup_result result;
	result.error = nullptr;
	result.ret = DICT_COMMIT_RET_OK;
	result.value = i_strdup(value.c_str());

	*((char**) dr->getContext()) = i_strdup(value.c_str());
	if (dr->getCallback() != nullptr) {
		dr->getCallback()(&result, dr->getContext());
	}
}

void safe_callback(rados_completion_t comp, void *arg) {
	i_debug("**** safe_callback ****");
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
	i_debug("rados_dict_init(uri=%s)", uri);

	dict = i_new(struct rados_dict, 1);
	dict->dr = new DictRados();

	int ret = dict->dr->init(uri, error_r);

	if (ret < 0) {
		delete dict->dr;
		i_free(dict);
		return -1;
	}

	dict->dict = *driver;

	if (strchr(set->username, DICT_USERNAME_SEPARATOR) == NULL) {
		dict->dr->setUsername(set->username);
	} else {
		/* escape the user name */
		dict->dr->setUsername(rados_escape_username(set->username));
	}

	dict->dr->ioContextSetNamspace(dict->dr->getUsername());
	i_debug("setIOContextNamespace(%s)", dict->dr->getUsername().c_str());

	*dict_r = &dict->dict;
	return 0;
}

void rados_dict_deinit(struct dict *_dict) {
	struct rados_dict *dict = (struct rados_dict *) _dict;

	i_debug("rados_dict_deinit");

	if (dict->dr != nullptr) {
		dict->dr->deinit();
		delete dict->dr;
		dict->dr = nullptr;
	}

	i_free(dict);
}

int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	int ret = DICT_COMMIT_RET_NOTFOUND;
	*value_r = NULL;

	i_debug("rados_dict_lookup(%s)", key);

	rados_dict_lookup_async(_dict, key, nullptr, value_r);
	dict->dr->waitForCompletion();
	if (*value_r != NULL && strlen(*value_r) > 0) {
		ret = DICT_COMMIT_RET_OK;
	}

	return ret;
}

void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	DictRados *pdr = dict->dr;
	int r_val = -1;
	int ret = DICT_COMMIT_RET_NOTFOUND;

	i_debug("rados_dict_lookup_async_1(%s)", key);
	set<string> keys;
	keys.insert(key);
	pdr->setLookupKey(key);
	pdr->setContext(context);
	pdr->setCallback(callback);
	pdr->clearReaderMap();
	pdr->getReadOperation().omap_get_vals_by_keys(keys, &pdr->getReaderMap(), &r_val);

	AioCompletion* completion = pdr->createCompletion(dict->dr, complete_callback, safe_callback);
	pdr->clearBufferList();
	int err = pdr->ioContextAioReadOperate(completion, &pdr->getReadOperation(), LIBRADOS_OPERATION_NOFLAG, &pdr->getBufferList());
	i_debug("rados_dict_lookup_async_2(%d)", err);
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

void rados_transaction_rollback(struct dict_transaction_context *_ctx) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;

	if (ctx->write_op) {
		delete ctx->write_op;
		ctx->write_op = nullptr;
	}

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

bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;

	*key_r = NULL;
	*value_r = NULL;

	if (iter->error != NULL)
		return FALSE;

	struct rados_dict *dict = (struct rados_dict *) ctx->dict;
	if (dict->dr->isEndReaderMapIterator()) {
		return FALSE;
	} else {
		i_debug("Iterator found key = '%s', value = '%s'", dict->dr->getReaderMapIter()->first.c_str(),
				dict->dr->getReaderMapIter()->second.to_str().c_str());
		*key_r = i_strdup(dict->dr->getReaderMapIter()->first.c_str());
		if ((iter->flags & DICT_ITERATE_FLAG_NO_VALUE) == 0) {
			*value_r = i_strdup(dict->dr->getReaderMapIter()->second.to_str().c_str());
		}
		dict->dr->incrementReaderMapIterator();
	}

	return TRUE;
}

int rados_dict_iterate_deinit(struct dict_iterate_context *ctx) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;
	int ret = iter->error != NULL ? -1 : 0;

	i_free(iter->error);
	i_free(iter);
	return ret;
}
