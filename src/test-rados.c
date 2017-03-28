#include "lib.h"
#include "test-common.h"
#include "ioloop.h"
#include "lib-signals.h"
#include "master-service.h"
#include "settings-parser.h"
#include "safe-mkstemp.h"
#include "safe-mkdir.h"
#include "str.h"
#include "unlink-directory.h"
#include "randgen.h"
#include "dcrypt.h"
#include "hex-binary.h"
#include "dict.h"
#include "dict-private.h"

#include "librados-plugin.h"

static const char* OMAP_KEY = "key";
static const char* OMAP_NO_KEY = "no_key";
static const char* OMAP_VALUE = "Artemis";

static char* OMAP_ITERATE_KEYS[] = { "K1", "K2", "K3", "K4", NULL };
static char* OMAP_ITERATE_VALUES[] = { "V1", "V2", "V3", "V4", NULL };

static struct ioloop *test_ioloop = NULL;
static pool_t test_pool;

static char * uri = "oid=metadata:pool=librmb-index:config=/home/peter/dovecot/etc/ceph/ceph.conf";
//static char * uri = "oid=metadata:pool=rbd:config=/home/peter/dovecot/etc/ceph/ceph.conf";

extern struct dict dict_driver_rados;

struct dict *test_dict_r = NULL;

static void dict_transaction_commit_sync_callback(int ret, void *context) {
	int *sync_result = context;

	*sync_result = ret;
}

static int pending = 0;

static void lookup_callback(const struct dict_lookup_result *result, void *context) {
	if (result->error != NULL)
		i_error("%s", result->error);
	else if (result->ret == 0)
		i_info("not found");
	else {
		i_info("%s", result->value);
		if (context != NULL) {
			*((char**) context) = result->value;
		}
	}
	pending--;
}

static void test_setup(void) {
	test_pool = pool_alloconly_create(MEMPOOL_GROWING "mcp test pool", 128);
	test_ioloop = io_loop_create();
	rados_plugin_init(NULL);
}

static void test_dict_init(void) {
	const char *error_r;

	struct dict_settings
	*set = i_new(struct dict_settings, 1);
	set->username = "t";

	int err = dict_driver_rados.v.init(&dict_driver_rados, uri, set, &test_dict_r, &error_r);
	test_assert(err == 0);

}

static void test_dict_set_get_delete(void) {
	struct dict_transaction_context * ctx;

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	test_dict_r->v.set(ctx, OMAP_KEY, OMAP_VALUE);

	int result = 0;

	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);

	const char *value_r;
	int err = dict_driver_rados.v.lookup(test_dict_r, test_pool, OMAP_NO_KEY, &value_r);
	test_assert(err == 0);

	err = dict_driver_rados.v.lookup(test_dict_r, test_pool, OMAP_KEY, &value_r);
	test_assert(err == 1);
	test_assert(strcmp(OMAP_VALUE, value_r) == 0);

	value_r = "";
	dict_driver_rados.v.lookup_async(test_dict_r, OMAP_KEY, lookup_callback, &value_r);
	//test_assert(err == 1);
	test_assert(strcmp(OMAP_VALUE, value_r) == 0);

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	test_dict_r->v.unset(ctx, OMAP_KEY);
	i_zero(&result);
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);
}

static void test_dict_get(void) {
	const char *value_r;
	const char *error_r;
	int err = dict_driver_rados.v.lookup(test_dict_r, test_pool, "XXX", &value_r);
	test_assert(err == DICT_COMMIT_RET_NOTFOUND);
}

static void test_dict_atomic_inc(void) {
	struct dict_transaction_context * ctx;
	int result = DICT_COMMIT_RET_NOTFOUND;
#if 0
	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	test_dict_r->v.atomic_inc(ctx, OMAP_KEY, 10);

	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	test_dict_r->v.unset(ctx, OMAP_KEY);
	result = DICT_COMMIT_RET_NOTFOUND;
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);
#endif
}

static void test_dict_iterate(void) {
	struct dict_transaction_context * ctx;
	int result;
	int i;

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	for (i = 0; i < 4; i++) {
		test_dict_r->v.set(ctx, OMAP_ITERATE_KEYS[i], OMAP_ITERATE_VALUES[i]);
	}
	result = DICT_COMMIT_RET_NOTFOUND;
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);

	const char *value_r;
	const char *error_r;

	struct dict_iterate_context * iter = dict_driver_rados.v.iterate_init(test_dict_r, OMAP_ITERATE_KEYS,
			DICT_ITERATE_FLAG_NO_VALUE);

	const char *k, *v;
	char *error;
	i = 0;

	while (dict_iterate(iter, &k, &v)) {
		test_assert(strcmp(k, OMAP_ITERATE_KEYS[i]) == 0);
		test_assert(strcmp(v, OMAP_ITERATE_VALUES[i]) == 0);
		i++;
	}
	test_assert(dict_driver_rados.v.iterate_deinit(iter) == 0);

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	for (i = 0; i < 4; i++) {
		test_dict_r->v.unset(ctx, OMAP_ITERATE_KEYS[i]);
	}
	result = DICT_COMMIT_RET_NOTFOUND;
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);
}

static void test_dict_deinit(void) {
	dict_driver_rados.v.deinit(test_dict_r);
}

static void test_teardown(void) {
	rados_plugin_deinit();
	io_loop_destroy(&test_ioloop);
	pool_unref(&test_pool);
}

int main(int argc, char **argv) {
	void (*tests[])(void) = {
		test_setup,
		test_dict_init,
		test_dict_get,
		test_dict_set_get_delete,
		test_dict_atomic_inc,
		test_dict_iterate,
		test_dict_deinit,
		test_teardown,
		NULL
	};

	master_service = master_service_init("test-rados",
			MASTER_SERVICE_FLAG_STANDALONE | MASTER_SERVICE_FLAG_NO_CONFIG_SETTINGS | MASTER_SERVICE_FLAG_NO_SSL_INIT, &argc, &argv,
			"");
	random_init();
	int ret = test_run(tests);
	master_service_deinit(&master_service);
	return ret;
}
