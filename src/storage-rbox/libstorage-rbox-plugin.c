/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "lib.h"
#include "config.h"
#include "mail-storage.h"

#include "index-storage.h"
#include "index-mail.h"
#include "mail-copy.h"
#include "libstorage-rbox-plugin.h"

extern struct mail_storage rbox_storage;

static int refcount = 0;

void storage_rbox_plugin_init(struct module *module) {
  i_debug("storage_rbox_plugin_init refcount=%d", refcount);
  if (refcount++ > 0)
    return;
  i_debug("storage_rbox_plugin_init registers rbox_storage ");
  mail_storage_class_register(&rbox_storage);
}

void storage_rbox_plugin_deinit(void) {
  i_debug("storage_rbox_plugin_deinit refcount=%d", refcount);
  if (--refcount > 0)
    return;
  i_debug("storage_rbox_plugin_deinit unregisters rbox_storage ");
  mail_storage_class_unregister(&rbox_storage);
}

const char *storage_rbox_plugin_version = DOVECOT_ABI_VERSION;
