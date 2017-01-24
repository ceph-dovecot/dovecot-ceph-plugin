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

#include "librados-plugin.h"

#define DICT_USERNAME_SEPARATOR '/'

struct rados_dict {
	struct dict dict;
	char *username;
	char *cluster_name, *cluster_user, *pool, *oid, *config;
	rados_ioctx_t io;
	rados_t cluster;
};

struct rados_dict_transaction_context {
	struct dict_transaction_context ctx;
	rados_write_op_t op;
	bool atomic_inc_not_found;
};

struct rados_dict_iterate_context {
	struct dict_iterate_context ctx;
	rados_read_op_t omap_iter;
	enum dict_iterate_flags flags;
	char *error;
};

static rados_t cluster = NULL;

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

	i_debug("rados_dict_init(uri=%s)", uri);

	dict = i_new(struct rados_dict, 1);

	dict->config = i_strdup("/etc/ceph/ceph.conf");
	dict->cluster_name = i_strdup("ceph");
	dict->cluster_user = i_strdup("client.admin");
	dict->pool = i_strdup("librmb");

	args = t_strsplit(uri, ":");
	for (; *args != NULL; args++) {
		if (strncmp(*args, "oid=", 4) == 0) {
			i_free(dict->oid);
			dict->oid = i_strdup(*args + 4);
		} else if (strncmp(*args, "config=", 7) == 0) {
			i_free(dict->config);
			dict->config = i_strdup(*args + 7);
		} else if (strncmp(*args, "pool=", 5) == 0) {
			i_free(dict->pool);
			dict->pool = i_strdup(*args + 5);
		} else if (strncmp(*args, "cluster_name=", 13) == 0) {
			i_free(dict->cluster_name);
			dict->cluster_name = i_strdup(*args + 13);
		} else if (strncmp(*args, "cluster_user=", 13) == 0) {
			i_free(dict->cluster_user);
			dict->cluster_user = i_strdup(*args + 13);
		} else {
			*error_r = t_strdup_printf("Unknown parameter: %s", *args);
			ret = -1;
		}
	}

	int err;

	if (ret >= 0) {
		uint64_t flags = 0;
		err = rados_create2(&dict->cluster, dict->cluster_name, dict->cluster_user, flags);
		i_debug("rados_create2(cluster_name=%s,cluster_user=%s)=%d", dict->cluster_name, dict->cluster_user, err);
		if (err < 0) {
			*error_r = t_strdup_printf("Couldn't create the cluster handle! %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = rados_conf_read_file(dict->cluster, (const char *) dict->config);
		i_debug("rados_conf_read_file(file=%s)=%d", dict->config, err);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot read config file: %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = rados_connect(dict->cluster);
		i_debug("rados_connect()=%d", err);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot connect to cluster: %s", strerror(-err));
			ret = -1;
		}
	}

	if (ret >= 0) {
		err = rados_ioctx_create(dict->cluster, dict->pool, &dict->io);
		i_debug("rados_ioctx_create(pool=%s)=%d", dict->pool, err);
		if (err < 0) {
			*error_r = t_strdup_printf("Cannot open RADOS pool %s: %s", dict->pool, strerror(-err));
			rados_shutdown(dict->cluster);
			ret = -1;
		}
	}

	if (ret < 0) {
		i_free(dict->config);
		i_free(dict->pool);
		i_free(dict->cluster_name);
		i_free(dict->cluster_user);
		i_free(dict);
		return -1;
	}

	dict->dict = *driver;

	if (strchr(set->username, DICT_USERNAME_SEPARATOR) == NULL)
		dict->username = i_strdup(set->username);
	else {
		/* escape the username */
		dict->username = i_strdup(rados_escape_username(set->username));
	}

	rados_ioctx_set_namespace(dict->io, (const char *) dict->username);
	i_debug("rados_ioctx_set_namespace(%s)", dict->username);

	*dict_r = &dict->dict;
	return 0;
}

static void rados_dict_deinit(struct dict *_dict) {
	struct rados_dict *dict = (struct rados_dict *) _dict;

	i_debug("rados_dict_deinit");

	rados_ioctx_destroy(dict->io);

	rados_shutdown(dict->cluster);

	i_free(dict->config);
	i_free(dict->pool);
	i_free(dict->cluster_name);
	i_free(dict->cluster_user);

	i_free(dict->username);
	i_free(dict);
}

static int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r, const char **error_r) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	rados_omap_iter_t iter;
	int r_val = -1;
	int ret = DICT_COMMIT_RET_NOTFOUND;
	*value_r = NULL;
	*error_r = NULL;

	i_debug("rados_dict_lookup(%s)", key);

	rados_write_op_t rop = rados_create_read_op();
	rados_read_op_omap_get_vals_by_keys(rop, &key, 1, &iter, &r_val);
	int err = rados_read_op_operate(rop, dict->io, dict->oid, LIBRADOS_OPERATION_NOFLAG);
	i_debug("rados_read_op_operate(namespace=%s,oid=%s)=%d(%s),%d(%s)", dict->username, dict->oid, err, strerror(-err), r_val,
			strerror(-r_val));

	if (err == 0) {
		if (r_val == 0) {
			char *omap_key = NULL;
			char *omap_val = NULL;
			size_t omap_val_len = 0;

			do {
				err = rados_omap_get_next(iter, &omap_key, &omap_val, &omap_val_len);
				if (err == 0&& !(omap_val_len == 0 && omap_key == NULL && omap_val == NULL)
				&& strcmp(key, omap_key) == 0 && omap_val != NULL) {
					*value_r = p_strndup(pool, omap_val, omap_val_len - 1);
					ret = DICT_COMMIT_RET_OK;
				}
			} while (err == 0 && !(omap_val_len == 0 && omap_key == NULL && omap_val == NULL));
		} else {
			ret = DICT_COMMIT_RET_FAILED;
			*error_r = t_strdup_printf("rados_read_op_omap_get_vals_by_keys(%s) failed: %d", key, r_val);
		}
	}
	if (err == -ENOENT) {
		ret = DICT_COMMIT_RET_NOTFOUND;
	} else {
		ret = DICT_COMMIT_RET_FAILED;
		*error_r = t_strdup_printf("rados_read_op_operate() failed: %d", err);
	}

	rados_release_read_op(rop);
	return ret;
}

static int rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context) {
	struct rados_dict *dict = (struct rados_dict *) _dict;
	rados_omap_iter_t iter;
	int r_val = -1;
	int ret = DICT_COMMIT_RET_NOTFOUND;

	i_debug("rados_dict_lookup(%s)", key);

	rados_write_op_t rop = rados_create_read_op();
	rados_read_op_omap_get_vals_by_keys(rop, &key, 1, &iter, &r_val);

	rados_completion_t completion;
	rados_aio_create_completion(NULL, NULL, NULL, &completion);

	int err = rados_aio_read_op_operate(rop, dict->io, completion, dict->oid, LIBRADOS_OPERATION_NOFLAG);

	rados_aio_wait_for_complete(completion);
	rados_aio_get_return_value(completion);
	rados_aio_release(completion);

	if (err < 0) {
		ret = DICT_COMMIT_RET_FAILED;
	} else {
		if (r_val == 0) {
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
			ret = DICT_COMMIT_RET_FAILED;
		}
	}

	rados_release_read_op(rop);
	return ret;
}

static struct dict_transaction_context *rados_transaction_init(struct dict *_dict) {
	struct rados_dict_transaction_context *ctx;

	ctx = i_new(struct rados_dict_transaction_context, 1);
	ctx->ctx.dict = _dict;
	ctx->op = rados_create_write_op();

	return &ctx->ctx;
}

static void rados_transaction_commit(struct dict_transaction_context *_ctx, bool async_ATTR_UNUSED,
		dict_transaction_commit_callback_t *callback, void *context) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	struct rados_dict *dict = (struct rados_dict *) _ctx->dict;
	struct dict_commit_result result;

	i_zero(&result);
	result.ret = DICT_COMMIT_RET_OK;

	if (ctx->op) {
		if (_ctx->changed) {
			int err = rados_write_op_operate(ctx->op, dict->io, dict->oid, NULL, LIBRADOS_OPERATION_NOFLAG);
			rados_release_write_op(ctx->op);

			if (err < 0)
				result.ret = DICT_COMMIT_RET_FAILED;
			else if (ctx->atomic_inc_not_found)
				result.ret = DICT_COMMIT_RET_NOTFOUND; // TODO DICT_COMMIT_RET_NOTFOUND = dict_atomic_inc() was used on a nonexistent key
			else
				result.ret = DICT_COMMIT_RET_OK;
		}
	}

	callback(&result, context);

	i_free(ctx);
}

static void rados_transaction_rollback(struct dict_transaction_context *_ctx) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;

	if (ctx->op) {
		rados_release_write_op(ctx->op);
	}

	i_free(ctx);
}

static void rados_set(struct dict_transaction_context *_ctx, const char *key, const char *value) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	i_debug("rados_set(%s,%s)", key, value);

	if (ctx->op) {
		const size_t len = strlen(value) + 1;  // store complete cstr
		rados_write_op_omap_set(ctx->op, &key, &value, &len, 1);
		_ctx->changed = TRUE;
	}
}

static void rados_unset(struct dict_transaction_context *_ctx, const char *key) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	i_debug("rados_unset(%s)", key);

	if (ctx->op) {
		rados_write_op_omap_rm_keys(ctx->op, &key, 1);
		_ctx->changed = TRUE;
	}
}

static void rados_atomic_inc(struct dict_transaction_context *_ctx, const char *key, long long diff) {
	struct rados_dict_transaction_context *ctx = (struct rados_dict_transaction_context *) _ctx;
	i_debug("rados_atomic_inc(%s,%lld)", key, diff);

	if (ctx->op) {
		string_t *str = t_str_new(strlen(key) + 64);
		str_printfa(str, "%s;%lld", key, diff);
		rados_write_op_exec(ctx->op, "rmb", "atomic_inc", (const char *) str_data(str), str_len(str) + 1, NULL); // store complete cstr
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
	i_assert((flags & (DICT_ITERATE_FLAG_SORT_BY_KEY | DICT_ITERATE_FLAG_SORT_BY_VALUE)) == 0);

	iter = i_new(struct rados_dict_iterate_context, 1);
	iter->ctx.dict = _dict;
	iter->flags = flags;

	rados_read_op_t op = rados_create_read_op();
	rados_read_op_omap_get_vals_by_keys(op, paths, str_array_length(paths), &iter->omap_iter, &rval);
	int err = rados_read_op_operate(op, dict->io, dict->oid, 0);

	if (err < 0) {
		iter->error = t_strdup_printf("rados_read_op_operate() failed: %d", err);
	}

	if (rval < 0) {
		iter->error = t_strdup_printf("rados_read_op_omap_get_vals_by_keys() failed: %d", rval);
	}

	if (op != NULL) {
		rados_release_read_op(op);
	}

	return &iter->ctx;
}

static bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;

	if (iter->error != NULL || iter->omap_iter == NULL)
		return FALSE;

	char *omap_key, *omap_val = NULL;
	size_t omap_val_len = 0;

	int err = rados_omap_get_next(iter->omap_iter, &omap_key, &omap_val, &omap_val_len);

	if (err < 0) {
		iter->error = i_strdup_printf("Failed to perform RADOS omap iteration: %d", err);
		return FALSE;
	}

	if (omap_key == NULL && omap_val == NULL && omap_val_len == 0) {
		// end of list
		rados_omap_get_end(iter->omap_iter);
		iter->omap_iter = NULL;
		return FALSE;
	}

	*key_r = omap_key; // TODO t_strdup(omap_key);

	if ((iter->flags & DICT_ITERATE_FLAG_NO_VALUE) != 0) {
		*value_r = NULL;
	}

	*value_r = omap_val; // TODO t_strdup(omap_val);

	return TRUE;
}

static int rados_dict_iterate_deinit(struct dict_iterate_context *ctx, const char **error_r) {
	struct rados_dict_iterate_context *iter = (struct rados_dict_iterate_context *) ctx;
	int ret = iter->error != NULL ? -1 : 0;
	*error_r = t_strdup(iter->error);

	i_free(iter->error);
	i_free(iter);
	return ret;
}

struct dict dict_driver_rados = { .name = "rados", {
		.init = rados_dict_init,
		.deinit = rados_dict_deinit,
		.wait = NULL,
		.lookup = rados_dict_lookup,
		.iterate_init = rados_dict_iterate_init,
		.iterate = rados_dict_iterate,
		.iterate_deinit = rados_dict_iterate_deinit,
		.transaction_init = rados_transaction_init,
		.transaction_commit = rados_transaction_commit,
		.transaction_rollback = rados_transaction_rollback,
		.set = rados_set,
		.unset = rados_unset,
		.atomic_inc = rados_atomic_inc,
		.lookup_async = NULL,
		.switch_ioloop = NULL,
		.set_timestamp = NULL } };

static int refcount = 0;

void rados_plugin_init(struct module *module ATTR_UNUSED) {
	i_debug("rados_plugin_init refcount=%d", refcount);
	if (refcount++ > 0)
		return;
	i_debug("rados_plugin_init registers dict_driver_rados ");
	dict_driver_register(&dict_driver_rados);
}

void rados_plugin_deinit(void) {
	i_debug("rados_plugin_deinit refcount=%d)", refcount);
	if (--refcount > 0)
		return;
	i_debug("rados_plugin_deinit unregisters dict_driver_rados ");
	dict_driver_unregister(&dict_driver_rados);
}

const char *rados_plugin_version = DOVECOT_ABI_VERSION;

