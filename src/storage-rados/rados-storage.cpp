/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <sys/stat.h>

#include <string>

#include <rados/librados.hpp>

extern "C" {

#include "lib.h"
#include "typeof-def.h"

#include "mail-copy.h"
#include "index-mail.h"
#include "mail-index-modseq.h"
#include "mailbox-list-private.h"
#include "index-pop3-uidl.h"

#include "rados-storage-local.h"
#include "rados-sync.h"
#include "debug-helper.h"
}

#include "rados-storage-struct.h"
#include "rados-cluster.h"
#include "rados-storage.h"
#include "rados-mail.h"

using namespace librados;  // NOLINT
using namespace tallence::librmb;

using std::string;

extern struct mail_storage rados_storage;
extern struct mailbox rados_mailbox;
extern struct mailbox_vfuncs rados_mailbox_vfuncs;

struct mail_storage *rados_storage_alloc(void) {
  FUNC_START();
  struct rados_storage *storage;
  pool_t pool;

  pool = pool_alloconly_create("rados storage", 512 + 256);
  storage = p_new(pool, struct rados_storage, 1);
  storage->storage = rados_storage;
  storage->storage.pool = pool;
  debug_print_mail_storage(&storage->storage, "rados-storage::rados_storage_alloc", NULL);
  FUNC_END();
  return &storage->storage;
}

void rados_storage_get_list_settings(const struct mail_namespace *ns ATTR_UNUSED, struct mailbox_list_settings *set) {
  FUNC_START();
  if (set->layout == NULL) {
    set->layout = MAILBOX_LIST_NAME_FS;
    // set->layout = MAILBOX_LIST_NAME_INDEX;
  }
  if (*set->maildir_name == '\0')
    set->maildir_name = RADOS_MAILDIR_NAME;
  if (*set->mailbox_dir_name == '\0')
    set->mailbox_dir_name = RADOS_MAILBOX_DIR_NAME;
  if (set->subscription_fname == NULL)
    set->subscription_fname = RADOS_SUBSCRIPTION_FILE_NAME;
  debug_print_mailbox_list_settings(set, "rados-storage::rados_storage_get_list_settings", NULL);
  FUNC_END();
}

static int rados_storage_create(struct mail_storage *_storage, struct mail_namespace *ns, const char **error_r) {
  FUNC_START();
  struct rados_storage *storage = (struct rados_storage *)_storage;

  string error_msg;
  int ret = storage->cluster.init(&error_msg);

  if (ret < 0) {
    // TODO(peter) free rados_storage?
    *error_r = t_strdup_printf("%s", error_msg.c_str());
    return -1;
  }

  string username = "unknown";
  if (storage->storage.user != NULL) {
    username = storage->storage.user->username;
  }

  string poolname = "mail_storage";
  ret = storage->cluster.storage_create(poolname, username, "my_oid", &storage->s);

  if (ret < 0) {
    *error_r = t_strdup_printf("Error creating RadosStorage()! %s", strerror(-ret));
    storage->cluster.deinit();
    return -1;
  }

  FUNC_END();
  return 0;
}

static void rados_storage_destroy(struct mail_storage *_storage) {
  FUNC_START();
  struct rados_storage *storage = (struct rados_storage *)_storage;

  storage->cluster.deinit();
  delete storage->s;
  storage->s = nullptr;

  index_storage_destroy(_storage);

  FUNC_END();
}

struct mailbox *rados_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname,
                                    enum mailbox_flags flags) {
  FUNC_START();
  struct rados_mailbox *mbox;
  struct index_mailbox_context *ibox;
  pool_t pool;

  /* rados can't work without index files */
  int intflags = flags & ~MAILBOX_FLAG_NO_INDEX_FILES;

  if (storage->set != NULL) {
    i_debug("mailbox_list_index = %s", btoa(storage->set->mailbox_list_index));
  }

  pool = pool_alloconly_create("rados mailbox", 1024 * 3);
  mbox = p_new(pool, struct rados_mailbox, 1);
  rados_mailbox.v = rados_mailbox_vfuncs;
  mbox->box = rados_mailbox;
  mbox->box.pool = pool;
  mbox->box.storage = storage;
  mbox->box.list = list;
  mbox->box.v = rados_mailbox_vfuncs;
  mbox->box.mail_vfuncs = &rados_mail_vfuncs;

  index_storage_mailbox_alloc(&mbox->box, vname, static_cast<mailbox_flags>(intflags), MAIL_INDEX_PREFIX);

  ibox = static_cast<index_mailbox_context *>(INDEX_STORAGE_CONTEXT(&mbox->box));
  intflags = ibox->index_flags | MAIL_INDEX_OPEN_FLAG_KEEP_BACKUPS | MAIL_INDEX_OPEN_FLAG_NEVER_IN_MEMORY;
  ibox->index_flags = static_cast<mail_index_open_flags>(intflags);

  mbox->storage = (struct rados_storage *)storage;

  i_debug("list name = %s", list->name);
  debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_alloc", NULL);
  FUNC_END();
  return &mbox->box;
}

static int rados_mailbox_alloc_index(struct rados_mailbox *mbox) {
  struct sdbox_index_header hdr;

  if (index_storage_mailbox_alloc_index(&mbox->box) < 0)
    return -1;

  mbox->hdr_ext_id = mail_index_ext_register(mbox->box.index, "dbox-hdr", sizeof(struct sdbox_index_header), 0, 0);
  /* set the initialization data in case the mailbox is created */
  i_zero(&hdr);
  guid_128_generate(hdr.mailbox_guid);
  mail_index_set_ext_init_data(mbox->box.index, mbox->hdr_ext_id, &hdr, sizeof(hdr));

  memcpy(mbox->mailbox_guid, hdr.mailbox_guid, sizeof(mbox->mailbox_guid));

  // register index record holding the mail guid
  mbox->ext_id = mail_index_ext_register(mbox->box.index, "obox", 0, sizeof(struct obox_mail_index_record), 1);

  return 0;
}

int rados_mailbox_open(struct mailbox *box) {
  FUNC_START();
  struct rados_mailbox *mbox = (struct rados_mailbox *)box;

  if (rados_mailbox_alloc_index(mbox) < 0)
    return -1;

  const char *box_path = mailbox_get_path(box);
  struct stat st;

  i_debug("rados-storage::rados_mailbox_open box_path = %s", box_path);

  if (stat(box_path, &st) == 0) {
    /* exists, open it */
  } else if (errno == ENOENT) {
    mail_storage_set_error(box->storage, MAIL_ERROR_NOTFOUND, T_MAIL_ERR_MAILBOX_NOT_FOUND(box->vname));
    debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  } else if (errno == EACCES) {
    mail_storage_set_critical(box->storage, "%s", mail_error_eacces_msg("stat", box_path));
    debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 2)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  } else {
    mail_storage_set_critical(box->storage, "stat(%s) failed: %m", box_path);
    debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 3)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  if (index_storage_mailbox_open(box, FALSE) < 0) {
    debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 4)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  mail_index_set_fsync_mode(
      box->index, box->storage->set->parsed_fsync_mode,
      static_cast<mail_index_fsync_mask>(MAIL_INDEX_FSYNC_MASK_APPENDS | MAIL_INDEX_FSYNC_MASK_EXPUNGES));

  debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open", NULL);
  FUNC_END();
  return 0;
}

static void rados_mailbox_close(struct mailbox *box) {
  FUNC_START();
  debug_print_mailbox(box, "rados_mailbox_close", NULL);
  index_storage_mailbox_close(box);
  FUNC_END();
}

int rados_mailbox_create(struct mailbox *box, const struct mailbox_update *update, bool directory) {
  FUNC_START();
  struct rados_mailbox *mbox = (struct rados_mailbox *)box;
  int ret;

  if ((ret = index_storage_mailbox_create(box, directory)) <= 0) {
    debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_create (ret <= 0, 1)", NULL);
    FUNC_END_RET("ret < 0");
    return ret;
  }

  ret = update == NULL ? 0 : index_storage_mailbox_update(box, update);

  i_debug("mailbox update = %p", update);
  debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_create", NULL);
  FUNC_END();
  return ret;
}

static int rados_mailbox_update(struct mailbox *box, const struct mailbox_update *update) {
  FUNC_START();
  debug_print_mailbox(box, "rados_mailbox_update", NULL);

  if (!box->opened) {
    if (mailbox_open(box) < 0)
      return -1;
  }

  // TODO(peter): if (sdbox_mailbox_create_indexes(box, update, NULL) < 0) return -1;

  int ret = index_storage_mailbox_update(box, update);
  FUNC_END();
  return ret;
}

int rados_mailbox_get_metadata(struct mailbox *box, enum mailbox_metadata_items items,
                               struct mailbox_metadata *metadata_r) {
  FUNC_START();
  struct rados_mailbox *mbox = (struct rados_mailbox *)box;

  debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_get_metadata", NULL);

  if (items != 0) {
    if (index_mailbox_get_metadata(box, items, metadata_r) < 0) {
      debug_print_mailbox(box, "rados-storage::rados_mailbox_get_metadata (ret -1, 1)", NULL);
      FUNC_END_RET("ret == -1");
      return -1;
    }
  }

  if ((items & MAILBOX_METADATA_GUID) != 0) {
    memcpy(metadata_r->guid, mbox->mailbox_guid, sizeof(metadata_r->guid));
  }

  if (metadata_r != NULL && metadata_r->cache_fields != NULL) {
    i_debug("metadata size = %lu", metadata_r->cache_fields->arr.element_size);
  }

  debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_get_metadata", NULL);
  debug_print_mailbox_metadata(metadata_r, "rados-storage::rados_mailbox_get_metadata", NULL);

  FUNC_END();
  return 0;
}

void rados_notify_changes(struct mailbox *box) {
  FUNC_START();
  struct rados_mailbox *mbox = (struct rados_mailbox *)box;

  if (box->notify_callback == NULL)
    mailbox_watch_remove_all(box);
  else
    mailbox_watch_add(box, mailbox_get_path(box));
  debug_print_rados_mailbox(mbox, "rados-storage::rados_notify_changes", NULL);
  FUNC_END();
}

int rados_mail_copy(struct mail_save_context *_ctx, struct mail *mail) {
  FUNC_START();
  struct rados_save_context *ctx = (struct rados_save_context *)_ctx;

  debug_print_mail(mail, "rados_mail_copy", NULL);
  debug_print_mail_save_context(_ctx, "rados_mail_copy", NULL);

  int ret = mail_storage_copy(_ctx, mail);

  FUNC_END();
  return ret;
}

struct mail_storage rados_storage = {
    .name = RADOS_STORAGE_NAME,
    .class_flags = static_cast<mail_storage_class_flags>(
        MAIL_STORAGE_CLASS_FLAG_FILE_PER_MSG | MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_GUIDS |
        MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_GUID128 | MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_SAVE_GUIDS |
        MAIL_STORAGE_CLASS_FLAG_BINARY_DATA),

    .v = {
        NULL, rados_storage_alloc, rados_storage_create, rados_storage_destroy, NULL, rados_storage_get_list_settings,
        NULL, rados_mailbox_alloc, NULL, NULL,
    }};

struct mailbox_vfuncs rados_mailbox_vfuncs = {index_storage_is_readonly,
                                              index_storage_mailbox_enable,
                                              index_storage_mailbox_exists,
                                              rados_mailbox_open,
                                              rados_mailbox_close,
                                              index_storage_mailbox_free,
                                              rados_mailbox_create,
                                              rados_mailbox_update,
                                              index_storage_mailbox_delete,
                                              index_storage_mailbox_rename,
                                              index_storage_get_status,
                                              rados_mailbox_get_metadata,
                                              index_storage_set_subscribed,
                                              index_storage_attribute_set,
                                              index_storage_attribute_get,
                                              index_storage_attribute_iter_init,
                                              index_storage_attribute_iter_next,
                                              index_storage_attribute_iter_deinit,
                                              index_storage_list_index_has_changed,
                                              index_storage_list_index_update_sync,
                                              rados_storage_sync_init,
                                              index_mailbox_sync_next,
                                              index_mailbox_sync_deinit,
                                              NULL,
                                              rados_notify_changes,
                                              index_transaction_begin,
                                              index_transaction_commit,
                                              index_transaction_rollback,
                                              NULL,
                                              rados_mail_alloc,
                                              index_storage_search_init,
                                              index_storage_search_deinit,
                                              index_storage_search_next_nonblock,
                                              index_storage_search_next_update_seq,
                                              rados_save_alloc,
                                              rados_save_begin,
                                              rados_save_continue,
                                              rados_save_finish,
                                              rados_save_cancel,
                                              rados_mail_copy,
                                              rados_transaction_save_commit_pre,
                                              rados_transaction_save_commit_post,
                                              rados_transaction_save_rollback,
                                              index_storage_is_inconsistent};

struct mailbox rados_mailbox = {};
