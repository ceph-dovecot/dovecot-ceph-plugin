/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "lib.h"
#include "dict-private.h"

#include "libdict-rados-plugin.h"
#include "dict-rados.h"

const char *dict_rados_plugin_version = DOVECOT_ABI_VERSION;

struct dict dict_driver_rados = {.name = "rados",
                                 {.init = rados_dict_init,
                                  .deinit = rados_dict_deinit,
                                  .wait = rados_dict_wait,
                                  .lookup = rados_dict_lookup,
                                  .iterate_init = rados_dict_iterate_init,
                                  .iterate = rados_dict_iterate,
                                  .iterate_deinit = rados_dict_iterate_deinit,
                                  .transaction_init = rados_dict_transaction_init,
                                  .transaction_commit = rados_dict_transaction_commit,
                                  .transaction_rollback = rados_dict_transaction_rollback,
                                  .set = rados_dict_set,
                                  .unset = rados_dict_unset,
                                  .atomic_inc = rados_dict_atomic_inc,
                                  .lookup_async = rados_dict_lookup_async,
                                  .switch_ioloop = NULL,
                                  .set_timestamp = rados_dict_set_timestamp}};

static int plugin_ref_count = 0;

void dict_rados_plugin_init(struct module *module ATTR_UNUSED) {
  i_info("%s v%s dictionary starting up", DOVECOT_RADOS_PLUGINS_PACKAGE_NAME, DOVECOT_RADOS_PLUGINS_PACKAGE_VERSION);
  if (plugin_ref_count++ > 0)
    return;
  dict_driver_register(&dict_driver_rados);
}

void dict_rados_plugin_deinit(void) {
  if (--plugin_ref_count > 0)
    return;
  dict_driver_unregister(&dict_driver_rados);
}
