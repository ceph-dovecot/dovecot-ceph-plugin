/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "lib.h"
#include "config.h"
#include "mail-storage.h"

#include "index-storage.h"
#include "index-mail.h"
#include "mail-copy.h"

#include "libstorage-rados-plugin.h"

extern struct mail_storage rados_storage;

static int refcount = 0;

void storage_rados_plugin_init(struct module *module) {
  i_debug("storage_rados_plugin_init refcount=%d", refcount);
  if (refcount++ > 0)
    return;
  i_debug("storage_rados_plugin_init registers rados_storage ");
  mail_storage_class_register(&rados_storage);
}

void storage_rados_plugin_deinit(void) {
  i_debug("storage_rados_plugin_deinit refcount=%d", refcount);
  if (--refcount > 0)
    return;
  i_debug("storage_rados_plugin_deinit unregisters rados_storage ");
  mail_storage_class_unregister(&rados_storage);
}

const char *storage_rados_plugin_version = DOVECOT_ABI_VERSION;
