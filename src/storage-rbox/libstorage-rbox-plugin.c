/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "lib.h"
#include "config.h"
#include "mail-storage.h"

#include "index-storage.h"
#include "index-mail.h"
#include "mail-copy.h"
#include "libstorage-rbox-plugin.h"

#include "rbox-storage.h"

static int refcount = 0;

struct mail_storage rbox_storage = {
    .name = "rbox",
    .class_flags = MAIL_STORAGE_CLASS_FLAG_FILE_PER_MSG | MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_GUIDS |
                   MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_GUID128 | MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_SAVE_GUIDS |
                   MAIL_STORAGE_CLASS_FLAG_BINARY_DATA,

    .v = {
        NULL, rbox_storage_alloc, rbox_storage_create, rbox_storage_destroy, NULL, rbox_storage_get_list_settings, NULL,
        rbox_mailbox_alloc, NULL, NULL,
    }};

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
