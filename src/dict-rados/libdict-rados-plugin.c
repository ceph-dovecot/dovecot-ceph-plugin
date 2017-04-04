#include "lib.h"
#include "config.h"
#include "array.h"
#include "fs-api.h"
#include "istream.h"
#include "str.h"
#include "dict-transaction-memory.h"
#include "dict-private.h"
#include "ostream.h"
#include "connection.h"
#include "module-dir.h"
#include "var-expand.h"
#include "config.h"
#include "config.h"
#include "config.h"

#include "libdict-rados-plugin.h"
#include "DictRados.h"

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
		.lookup_async = rados_dict_lookup_async,
		.switch_ioloop = NULL,
		.set_timestamp = NULL } };

static int refcount = 0;

void dict_rados_plugin_init(struct module *module) {
	i_debug("dict_rados_plugin_init refcount=%d", refcount);
	if (refcount++ > 0)
		return;
	i_debug("dict_rados_plugin_init registers dict_driver_rados ");
	dict_driver_register(&dict_driver_rados);
}

void dict_rados_plugin_deinit(void) {
	i_debug("dict_rados_plugin_deinit refcount=%d", refcount);
	if (--refcount > 0)
		return;
	i_debug("dict_rados_plugin_deinit unregisters dict_driver_rados ");
	dict_driver_unregister(&dict_driver_rados);
}

const char *dict_rados_plugin_version = DOVECOT_ABI_VERSION;
