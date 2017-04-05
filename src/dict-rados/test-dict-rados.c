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

#include "libdict-rados-plugin.h"
#include <rados/librados.h>

static const char* OMAP_KEY = "key";
static const char* OMAP_NO_KEY = "no_key";
static const char* OMAP_VALUE = "Artemis";

static char* OMAP_ITERATE_KEYS[] = { "K1", "K2", "K3", "K4", NULL };
static char* OMAP_ITERATE_VALUES[] = { "V1", "V2", "V3", "V4", NULL };

static char* OMAP_ITERATE_REC_KEYS[] = { "A1", "A1/B1", "A/B1/C1", "A/B1/C2", "A1/B2", "A2", NULL };
static char* OMAP_ITERATE_REC_VALUES[] = { "V-A1", "V-A1/B1", "V-A/B1/C1", "V-A/B1/C2", "V-A1/B2", "V-A2", NULL };

static const char *pool_name = "test_dict_rados";
static struct ioloop *test_ioloop = NULL;
static pool_t test_pool;

static char * uri = "oid=metadata:pool=test_dict_rados";

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
			*((char**) context) = i_strdup(result->value);
		}
	}
	pending--;
}

static void test_setup(void) {
	test_pool = pool_alloconly_create(MEMPOOL_GROWING "mcp test pool", 128);
	test_ioloop = io_loop_create();

	test_begin("dict_plugin_init");
	dict_rados_plugin_init(NULL);
	test_end();
}

static void test_dict_init(void) {
	const char *error_r;

	struct dict_settings *set = i_new(struct dict_settings, 1);
	set->username = "t";

	test_begin("dict_init");
	int err = dict_driver_rados.v.init(&dict_driver_rados, uri, set, &test_dict_r, &error_r);
	test_assert(err == 0);
	test_end();
}

static void test_dict_set_get_delete(void) {
	struct dict_transaction_context * ctx;

	test_begin("dict_set_get_delete");

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

	// err = dict_driver_rados.v.lookup_async(test_dict_r, OMAP_KEY, lookup_callback, NULL);
	// test_assert(err == 1);
	// test_assert(strcmp(OMAP_VALUE, value_r) == 0);

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	test_dict_r->v.unset(ctx, OMAP_KEY);
	i_zero(&result);
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);
	test_end();
}

static void test_dict_get_not_found(void) {
	const char *value_r;
	const char *error_r;

	test_begin("dict_get_not_found");
	int err = dict_driver_rados.v.lookup(test_dict_r, test_pool, "dict_get_not_found", &value_r);
	test_assert(err == DICT_COMMIT_RET_NOTFOUND);
	test_end();
}

static void test_dict_atomic_inc(void) {
	struct dict_transaction_context * ctx;
	int result = DICT_COMMIT_RET_NOTFOUND;
#if 0
	test_begin("dict_atomic_inc");
	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	test_dict_r->v.atomic_inc(ctx, OMAP_KEY, 10);

	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	test_dict_r->v.unset(ctx, OMAP_KEY);
	result = DICT_COMMIT_RET_NOTFOUND;
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);
	test_end();
#endif
}

static void test_dict_iterate(void) {
	struct dict_transaction_context * ctx;
	int result;
	int i;

	test_begin("dict_iterate");

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
			DICT_ITERATE_FLAG_SORT_BY_KEY);

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

	test_end();
}

static void test_dict_iterate_no_value(void) {
	struct dict_transaction_context * ctx;
	int result;
	int i;

	test_begin("test_dict_iterate_no_value");

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
		test_assert(v == 0);
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

	test_end();
}

static void test_dict_iterate_rec(void) {
	struct dict_transaction_context * ctx;
	int result;
	int i;
	const char** k;
	const char** v;
#if 0

	test_begin("dict_iterate_rec");

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	i = 0;
	for (k = OMAP_ITERATE_REC_KEYS; *k != NULL; k++) {
		test_dict_r->v.set(ctx, *k, OMAP_ITERATE_REC_VALUES[i++]);
	}
	result = DICT_COMMIT_RET_NOTFOUND;
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);

	const char *kr;
	const char *vr;
	const char *s4 = "A1";

	struct dict_iterate_context * iter = dict_driver_rados.v.iterate_init(test_dict_r, &s4, DICT_ITERATE_FLAG_RECURSE);

	i = 0;
	while (dict_iterate(iter, &kr, &vr)) {
		test_assert(strcmp(kr, OMAP_ITERATE_REC_KEYS[i]) == 0);
		test_assert(strcmp(vr, OMAP_ITERATE_REC_VALUES[i]) == 0);
		i++;
	}
	test_assert(dict_driver_rados.v.iterate_deinit(iter) == 0);

	ctx = dict_driver_rados.v.transaction_init(test_dict_r);
	for (k = OMAP_ITERATE_REC_KEYS; *k != NULL; k++) {
		test_dict_r->v.unset(ctx, *k);
	}
	result = DICT_COMMIT_RET_NOTFOUND;
	ctx->dict->v.transaction_commit(ctx, FALSE, dict_transaction_commit_sync_callback, &result);
	test_assert(result == DICT_COMMIT_RET_OK);

	test_end();
#endif
}

static void test_dict_deinit(void) {
	test_begin("dict_deinit");
	dict_driver_rados.v.deinit(test_dict_r);
	test_end();
}

static void test_teardown(void) {
	test_begin("dict_plugin_deinit");
	dict_rados_plugin_deinit();
	io_loop_destroy(&test_ioloop);
	pool_unref(&test_pool);
	test_end();
}

int main(int argc, char **argv) {
	void (*tests[])(void) = {
		test_setup,
		test_dict_init,
		test_dict_get_not_found,
		test_dict_set_get_delete,
		test_dict_atomic_inc,
		test_dict_iterate,
		test_dict_iterate_no_value,
		test_dict_iterate_rec,
		test_dict_deinit,
		test_teardown,
		NULL
	};

	// prepare Ceph
	int ret = 0;
	int pool_created = 0;
	rados_t rados = NULL;
	{
		ret = rados_create(&rados, NULL); // just use the client.admin keyring
		if (ret < 0) { // let's handle any error that might have come back
			printf("couldn't initialize rados! error %d\n", ret);
			ret = EXIT_FAILURE;
			goto out;
		}
	}
	ret = rados_conf_parse_env(rados, NULL);
	ret = rados_conf_read_file(rados, NULL);

	/*
	 * Now we need to get the rados object its config info. It can
	 * parse argv for us to find the id, monitors, etc, so let's just
	 * use that.
	 */
	{
		ret = rados_conf_parse_argv(rados, argc, argv);
		if (ret < 0) {
			// This really can't happen, but we need to check to be a good citizen.
			printf("failed to parse config options! error %d\n", ret);
			ret = EXIT_FAILURE;
			goto out;
		} else {
			// We also want to apply the config file if the user specified
			// one, and conf_parse_argv won't do that for us.
			int i;
			for (i = 0; i < argc; ++i) {
				if ((strcmp(argv[i], "-c") == 0) || (strcmp(argv[i], "--conf") == 0)) {
					ret = rados_conf_read_file(rados, argv[i + 1]);
					if (ret < 0) {
						// This could fail if the config file is malformed, but it'd be hard.
						printf("failed to parse config file %s! error %d\n", argv[i + 1], ret);
						ret = EXIT_FAILURE;
						goto out;
					}
					break;
				}
			}
		}
	}

	ret = rados_connect(rados);
	if (ret < 0) {
		printf("couldn't connect to cluster! error %d\n", ret);
		ret = EXIT_FAILURE;
		goto out;
	}

	/*
	 * let's create our own pool instead of scribbling over real data.
	 * Note that this command creates pools with default PG counts specified
	 * by the monitors, which may not be appropriate for real use -- it's fine
	 * for testing, though.
	 */
	ret = rados_pool_create(rados, pool_name);
	if (ret < 0 && ret != -EEXIST) {
		printf("couldn't create pool! error %d\n", ret);
		return EXIT_FAILURE;
	}
	pool_created = 1;

	// prepare Dovecot
	master_service = master_service_init("test-rados",
			MASTER_SERVICE_FLAG_STANDALONE | MASTER_SERVICE_FLAG_NO_CONFIG_SETTINGS | MASTER_SERVICE_FLAG_NO_SSL_INIT, &argc, &argv,
			"");
	random_init();

	// start tests
	ret = test_run(tests);

	// Dovecot exit
	master_service_deinit(&master_service);

	// Ceph  exit
	out: if (pool_created) {
		/*
		 * And now we're done, so let's remove our pool and then
		 * shut down the connection gracefully.
		 */
		int delete_ret = rados_pool_delete(rados, pool_name);
		if (delete_ret < 0) {
			// be careful not to
			printf("We failed to delete our test pool!\n");
			ret = EXIT_FAILURE;
		}
	}

	rados_shutdown(rados);

	return ret;

}
