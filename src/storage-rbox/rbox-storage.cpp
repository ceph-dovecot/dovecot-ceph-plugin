/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <sys/stat.h>

#include <string>

#include <rados/librados.hpp>

const char *SETTINGS_RBOX_POOL_NAME = "rbox_pool_name";
const char *SETTINGS_DEF_RADOS_POOL = "mail_storage";
const char *DEF_USERNAME = "unknown";

extern "C" {

#include "lib.h"
#include <typeof-def.h>

#include "mail-copy.h"
#include "index-mail.h"
#include "mail-index-modseq.h"
#include "mailbox-list-private.h"
#include "index-pop3-uidl.h"
#include "rbox-sync.h"
#include "debug-helper.h"
}

#include "rbox-storage.hpp"

#include "rados-cluster.h"
#include "rados-storage.h"
#include "rbox-copy.h"
#include "rbox-mail.h"

using namespace librados;  // NOLINT
using namespace librmb;    // NOLINT

using std::string;

extern struct mailbox rbox_mailbox;
extern struct mailbox_vfuncs rbox_mailbox_vfuncs;

struct mail_storage *rbox_storage_alloc(void) {
  FUNC_START();
  struct rbox_storage *storage;
  pool_t pool;

  pool = pool_alloconly_create("rbox storage", 512 + 256);
  storage = p_new(pool, struct rbox_storage, 1);
  storage->storage = rbox_storage;
  storage->storage.pool = pool;
  storage->s = nullptr;

  debug_print_mail_storage(&storage->storage, "rados-storage::rados_storage_alloc", NULL);
  FUNC_END();
  return &storage->storage;
}

void rbox_storage_get_list_settings(const struct mail_namespace *ns ATTR_UNUSED, struct mailbox_list_settings *set) {
  FUNC_START();
  if (set->layout == NULL) {
    set->layout = MAILBOX_LIST_NAME_FS;
    // TODO(peter): better default? set->layout = MAILBOX_LIST_NAME_INDEX;
  }
  if (*set->maildir_name == '\0')
    set->maildir_name = RBOX_MAILDIR_NAME;
  if (*set->mailbox_dir_name == '\0')
    set->mailbox_dir_name = RBOX_MAILBOX_DIR_NAME;
  if (set->subscription_fname == NULL)
    set->subscription_fname = RBOX_SUBSCRIPTION_FILE_NAME;
  debug_print_mailbox_list_settings(set, "rbox-storage::rbox_storage_get_list_settings", NULL);
  FUNC_END();
}

int rbox_storage_create(struct mail_storage *_storage, struct mail_namespace *ns, const char **error_r) {
  FUNC_START();
  struct rbox_storage *storage = (struct rbox_storage *)_storage;
  // RADOS initialization postponed to mailbox_open
  FUNC_END();
  return 0;
}

void rbox_storage_destroy(struct mail_storage *_storage) {
  FUNC_START();
  struct rbox_storage *storage = (struct rbox_storage *)_storage;

  storage->cluster.deinit();
  delete storage->s;
  storage->s = nullptr;

  index_storage_destroy(_storage);

  FUNC_END();
}

struct mailbox *rbox_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname,
                                   enum mailbox_flags flags) {
  FUNC_START();
  struct rbox_mailbox *mbox;
  struct index_mailbox_context *ibox;
  pool_t pool;

  /* rados can't work without index files */
  int intflags = flags & ~MAILBOX_FLAG_NO_INDEX_FILES;

  if (storage->set != NULL) {
    i_debug("mailbox_list_index = %s", btoa(storage->set->mailbox_list_index));
  }

  pool = pool_alloconly_create("rbox mailbox", 1024 * 3);
  mbox = p_new(pool, struct rbox_mailbox, 1);
  rbox_mailbox.v = rbox_mailbox_vfuncs;
  mbox->box = rbox_mailbox;
  mbox->box.pool = pool;
  mbox->box.storage = storage;
  mbox->box.list = list;
  mbox->box.v = rbox_mailbox_vfuncs;
  mbox->box.mail_vfuncs = &rbox_mail_vfuncs;

  i_debug("rbox_mailbox_alloc: vname = %s, storage-name = %s, mail-location = %s", vname, storage->name,
          storage->set->mail_location);

  index_storage_mailbox_alloc(&mbox->box, vname, static_cast<mailbox_flags>(intflags), MAIL_INDEX_PREFIX);

  ibox = static_cast<index_mailbox_context *>(INDEX_STORAGE_CONTEXT(&mbox->box));
  intflags = ibox->index_flags | MAIL_INDEX_OPEN_FLAG_KEEP_BACKUPS | MAIL_INDEX_OPEN_FLAG_NEVER_IN_MEMORY;
  ibox->index_flags = static_cast<mail_index_open_flags>(intflags);

  mbox->storage = (struct rbox_storage *)storage;

  i_debug("list name = %s", list->name);
  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_alloc", NULL);
  FUNC_END();
  return &mbox->box;
}

static int rbox_mailbox_alloc_index(struct rbox_mailbox *mbox) {
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

int rbox_read_header(struct rbox_mailbox *mbox, struct sdbox_index_header *hdr, bool log_error, bool *need_resize_r) {
  struct mail_index_view *view;
  const void *data;
  size_t data_size;
  int ret = 0;

  i_assert(mbox->box.opened);

  view = mail_index_view_open(mbox->box.index);
  mail_index_get_header_ext(view, mbox->hdr_ext_id, &data, &data_size);
  if (data_size < SDBOX_INDEX_HEADER_MIN_SIZE && (!mbox->box.creating || data_size != 0)) {
    if (log_error) {
      mail_storage_set_critical(&mbox->storage->storage, "sdbox %s: Invalid dbox header size",
                                mailbox_get_path(&mbox->box));
    }
    ret = -1;
  } else {
    i_zero(hdr);
    memcpy(hdr, data, I_MIN(data_size, sizeof(*hdr)));
    if (guid_128_is_empty(hdr->mailbox_guid)) {
      ret = -1;
    } else {
      /* data is valid. remember it in case mailbox
         is being reset */
      mail_index_set_ext_init_data(mbox->box.index, mbox->hdr_ext_id, hdr, sizeof(*hdr));
    }
  }
  mail_index_view_close(&view);
  *need_resize_r = data_size < sizeof(*hdr);
  return ret;
}

int rbox_mailbox_open(struct mailbox *box) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  struct sdbox_index_header hdr;
  bool need_resize;

  if (rbox_mailbox_alloc_index(mbox) < 0)
    return -1;

  const char *box_path = mailbox_get_path(box);
  struct stat st;

  i_debug("rbox-storage::rbox_mailbox_open box_path = %s", box_path);

  if (stat(box_path, &st) == 0) {
    /* exists, open it */
  } else if (errno == ENOENT) {
    mail_storage_set_error(box->storage, MAIL_ERROR_NOTFOUND, T_MAIL_ERR_MAILBOX_NOT_FOUND(box->vname));
    debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_open (ret -1, 1)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  } else if (errno == EACCES) {
    mail_storage_set_critical(box->storage, "%s", mail_error_eacces_msg("stat", box_path));
    debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_open (ret -1, 2)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  } else {
    mail_storage_set_critical(box->storage, "stat(%s) failed: %m", box_path);
    debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_open (ret -1, 3)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  if (index_storage_mailbox_open(box, FALSE) < 0) {
    debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_open (ret -1, 4)", NULL);
    FUNC_END_RET("ret == -1");
    return -1;
  }

  string error_msg;
  if (mbox->storage->cluster.init(&error_msg) < 0) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  if (mbox->storage->s == nullptr) {
    const char *poolname = SETTINGS_DEF_RADOS_POOL;
    const char *settings_poolname = mail_user_plugin_getenv(mbox->storage->storage.user, SETTINGS_RBOX_POOL_NAME);
    if (settings_poolname != nullptr && strlen(settings_poolname) > 0) {
      poolname = settings_poolname;
    }

    int ret = mbox->storage->cluster.storage_create(poolname, &mbox->storage->s);
    if (ret < 0) {
      mbox->storage->cluster.deinit();
      FUNC_END_RET("ret == -1");
      return -1;
    }
  }

  if (box->list->ns->owner != nullptr) {
    i_debug("Namespace owner : %s setting rados namespace", box->list->ns->owner->username);
    ((struct rbox_storage *)box->storage)->s->get_io_ctx().set_namespace(box->list->ns->owner->username);
  }

  mail_index_set_fsync_mode(
      box->index, box->storage->set->parsed_fsync_mode,
      static_cast<mail_index_fsync_mask>(MAIL_INDEX_FSYNC_MASK_APPENDS | MAIL_INDEX_FSYNC_MASK_EXPUNGES));

  /* get/generate mailbox guid */
  if (rbox_read_header(mbox, &hdr, FALSE, &need_resize) < 0) {
    /* looks like the mailbox is corrupted */
    // (void)rbox_sync(mbox, 0 /*SDBOX_SYNC_FLAG_FORCE*/);
    if (rbox_read_header(mbox, &hdr, TRUE, &need_resize) < 0)
      i_zero(&hdr);
  }

  if (guid_128_is_empty(hdr.mailbox_guid)) {
    /* regenerate it */
    //    if (rbox_mailbox_create_indexes(box, NULL, NULL) < 0 || rbox_read_header(mbox, &hdr, TRUE, &need_resize) < 0)
    //      return -1;
  }
  memcpy(mbox->mailbox_guid, hdr.mailbox_guid, sizeof(mbox->mailbox_guid));

  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_open", NULL);
  FUNC_END();
  return 0;
}

int rbox_mailbox_create(struct mailbox *box, const struct mailbox_update *update, bool directory) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  int ret;

  if ((ret = index_storage_mailbox_create(box, directory)) <= 0) {
    debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_create (ret <= 0, 1)", NULL);
    FUNC_END_RET("ret < 0");
    return ret;
  }

  ret = update == NULL ? 0 : index_storage_mailbox_update(box, update);

  i_debug("mailbox update = %p", update);
  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_create", NULL);
  FUNC_END();
  return ret;
}

static int rbox_mailbox_update(struct mailbox *box, const struct mailbox_update *update) {
  FUNC_START();
  debug_print_mailbox(box, "rbox_mailbox_update", NULL);

  if (!box->opened) {
    if (mailbox_open(box) < 0)
      return -1;
  }

  // TODO(peter): if (sdbox_mailbox_create_indexes(box, update, NULL) < 0) return -1;

  int ret = index_storage_mailbox_update(box, update);
  FUNC_END();
  return ret;
}

int rbox_mailbox_get_metadata(struct mailbox *box, enum mailbox_metadata_items items,
                              struct mailbox_metadata *metadata_r) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;

  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_get_metadata", NULL);

  if (items != 0) {
    if (index_mailbox_get_metadata(box, items, metadata_r) < 0) {
      debug_print_mailbox(box, "rbox-storage::rbox_mailbox_get_metadata (ret -1, 1)", NULL);
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

  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_get_metadata", NULL);
  debug_print_mailbox_metadata(metadata_r, "rbox-storage::rbox_mailbox_get_metadata", NULL);

  FUNC_END();
  return 0;
}

void rbox_notify_changes(struct mailbox *box) {
  FUNC_START();

  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;

  if (box->notify_callback == NULL)
    mailbox_watch_remove_all(box);
  else
    mailbox_watch_add(box, mailbox_get_path(box));

  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_notify_changes", NULL);
  FUNC_END();
}

struct mailbox_vfuncs rbox_mailbox_vfuncs = {index_storage_is_readonly,
                                             index_storage_mailbox_enable,
                                             index_storage_mailbox_exists,
                                             rbox_mailbox_open,
                                             index_storage_mailbox_close,
                                             index_storage_mailbox_free,
                                             rbox_mailbox_create,
                                             rbox_mailbox_update,
                                             index_storage_mailbox_delete,
                                             index_storage_mailbox_rename,
                                             index_storage_get_status,
                                             rbox_mailbox_get_metadata,
                                             index_storage_set_subscribed,
                                             index_storage_attribute_set,
                                             index_storage_attribute_get,
                                             index_storage_attribute_iter_init,
                                             index_storage_attribute_iter_next,
                                             index_storage_attribute_iter_deinit,
                                             index_storage_list_index_has_changed,
                                             index_storage_list_index_update_sync,
                                             rbox_storage_sync_init,
                                             index_mailbox_sync_next,
                                             index_mailbox_sync_deinit,
                                             NULL,
                                             rbox_notify_changes,
                                             index_transaction_begin,
                                             index_transaction_commit,
                                             index_transaction_rollback,
                                             NULL,
                                             rbox_mail_alloc,
                                             index_storage_search_init,
                                             index_storage_search_deinit,
                                             index_storage_search_next_nonblock,
                                             index_storage_search_next_update_seq,
                                             rbox_save_alloc,
                                             rbox_save_begin,
                                             rbox_save_continue,
                                             rbox_save_finish,
                                             rbox_save_cancel,
                                             rbox_mail_copy,
                                             rbox_transaction_save_commit_pre,
                                             rbox_transaction_save_commit_post,
                                             rbox_transaction_save_rollback,
                                             index_storage_is_inconsistent};

struct mailbox rbox_mailbox = {};
