// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include <sys/stat.h>
#include <dirent.h>

#include <string>

#include <rados/librados.hpp>

const char *SETTINGS_RBOX_POOL_NAME = "rbox_pool_name";
const char *SETTINGS_DEF_RADOS_POOL = "mail_storage";
const char *DEF_USERNAME = "unknown";

extern "C" {

#include "dovecot-all.h"

#include "rbox-sync.h"
#include "debug-helper.h"
}

#include "rbox-storage.hpp"

#include "rados-cluster.h"
#include "rados-storage.h"
#include "rbox-copy.h"
#include "rbox-mail.h"

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
  storage->cluster = new librmb::RadosClusterImpl();
  storage->s = new librmb::RadosStorageImpl(storage->cluster);

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

int rbox_storage_create(struct mail_storage *ATTR_UNUSED, struct mail_namespace *ATTR_UNUSED,
                        const char **ATTR_UNUSED) {
  FUNC_START();
  // RADOS initialization postponed to mailbox_open
  FUNC_END();
  return 0;
}

void rbox_storage_destroy(struct mail_storage *_storage) {
  FUNC_START();
  struct rbox_storage *storage = (struct rbox_storage *)_storage;

  storage->cluster->deinit();
  if (storage->s != nullptr) {
    delete storage->s;
    storage->s = nullptr;
  }
  if (storage->cluster != nullptr) {
    // delete cluster after storage!
    delete storage->cluster;
    storage->cluster = nullptr;
  }

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

static int rbox_open_mailbox(struct mailbox *box) {
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
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
  mail_index_set_fsync_mode(
      box->index, box->storage->set->parsed_fsync_mode,
      static_cast<mail_index_fsync_mask>(MAIL_INDEX_FSYNC_MASK_APPENDS | MAIL_INDEX_FSYNC_MASK_EXPUNGES));

  if (!array_is_created(&mbox->moved_items)) {
    i_array_init(&mbox->moved_items, 32);
  }

  return 0;
}

int rbox_open_rados_connection(struct mailbox *box) {
  /* rados cluster connection */
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  std::string ns(box->list->ns->owner != nullptr ? box->list->ns->owner->username : "");
  std::string poolname = SETTINGS_DEF_RADOS_POOL;
  const char *settings_poolname = mail_user_plugin_getenv(mbox->storage->storage.user, SETTINGS_RBOX_POOL_NAME);
  if (settings_poolname != nullptr && strlen(settings_poolname) > 0) {
    poolname = settings_poolname;
  }
  return mbox->storage->s->open_connection(poolname, ns);
}

void rbox_sync_update_header(struct index_rebuild_context *ctx) {
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)ctx->box;
  struct sdbox_index_header hdr;
  bool need_resize;

  if (rbox_read_header(mbox, &hdr, FALSE, &need_resize) < 0)
    i_zero(&hdr);
  if (guid_128_is_empty(hdr.mailbox_guid))
    guid_128_generate(hdr.mailbox_guid);
  if (++hdr.rebuild_count == 0)
    hdr.rebuild_count = 1;
  /* mailbox is being reset. this gets written directly there */
  mail_index_set_ext_init_data(ctx->box->index, mbox->hdr_ext_id, &hdr, sizeof(hdr));
}

static void rbox_update_header(struct rbox_mailbox *mbox, struct mail_index_transaction *trans,
                               const struct mailbox_update *update) {
  struct sdbox_index_header hdr, new_hdr;
  bool need_resize;

  if (rbox_read_header(mbox, &hdr, TRUE, &need_resize) < 0) {
    i_zero(&hdr);
    need_resize = TRUE;
  }

  new_hdr = hdr;

  if (update != NULL && !guid_128_is_empty(update->mailbox_guid)) {
    memcpy(new_hdr.mailbox_guid, update->mailbox_guid, sizeof(new_hdr.mailbox_guid));
  } else if (guid_128_is_empty(new_hdr.mailbox_guid)) {
    guid_128_generate(new_hdr.mailbox_guid);
  }

  if (need_resize) {
    mail_index_ext_resize_hdr(trans, mbox->hdr_ext_id, sizeof(new_hdr));
  }
  if (memcmp(&hdr, &new_hdr, sizeof(hdr)) != 0) {
    mail_index_update_header_ext(trans, mbox->hdr_ext_id, 0, &new_hdr, sizeof(new_hdr));
  }
  memcpy(mbox->mailbox_guid, new_hdr.mailbox_guid, sizeof(mbox->mailbox_guid));
}

uint32_t rbox_get_uidvalidity_next(struct mailbox_list *list) {
  const char *path;

  path = mailbox_list_get_root_forced(list, MAILBOX_LIST_PATH_TYPE_CONTROL);
  path = t_strconcat(path, "/" RBOX_UIDVALIDITY_FILE_NAME, NULL);
  return mailbox_uidvalidity_next(list, path);
}

int rbox_mailbox_create_indexes(struct mailbox *box, const struct mailbox_update *update,
                                struct mail_index_transaction *trans) {
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  struct mail_index_transaction *new_trans = NULL;
  const struct mail_index_header *hdr;
  uint32_t uid_validity, uid_next;

  if (trans == NULL) {
    new_trans = mail_index_transaction_begin(box->view, static_cast<mail_index_transaction_flags>(0));
    trans = new_trans;
  }

  hdr = mail_index_get_header(box->view);
  if (update != NULL && update->uid_validity != 0) {
    uid_validity = update->uid_validity;
  } else if (hdr->uid_validity != 0) {
    uid_validity = hdr->uid_validity;
  } else {
    /* set uidvalidity */
    uid_validity = rbox_get_uidvalidity_next(box->list);
  }

  if (hdr->uid_validity != uid_validity) {
    mail_index_update_header(trans, offsetof(struct mail_index_header, uid_validity), &uid_validity,
                             sizeof(uid_validity), TRUE);
  }
  if (update != NULL && hdr->next_uid < update->min_next_uid) {
    uid_next = update->min_next_uid;
    mail_index_update_header(trans, offsetof(struct mail_index_header, next_uid), &uid_next, sizeof(uid_next), TRUE);
  }
  if (update != NULL && update->min_first_recent_uid != 0 && hdr->first_recent_uid < update->min_first_recent_uid) {
    uint32_t first_recent_uid = update->min_first_recent_uid;

    mail_index_update_header(trans, offsetof(struct mail_index_header, first_recent_uid), &first_recent_uid,
                             sizeof(first_recent_uid), FALSE);
  }
  if (update != NULL && update->min_highest_modseq != 0 &&
      mail_index_modseq_get_highest(box->view) < update->min_highest_modseq) {
    mail_index_modseq_enable(box->index);
    mail_index_update_highest_modseq(trans, update->min_highest_modseq);
  }

#ifdef HAVE_INDEX_POP3_UIDL_H
  if (box->inbox_user && box->creating) {
    /* initialize pop3-uidl header when creating mailbox
       (not on mailbox_update()) */
    index_pop3_uidl_set_max_uid(box, trans, 0);
  }
#endif

  rbox_update_header(mbox, trans, update);
  if (new_trans != NULL) {
    if (mail_index_transaction_commit(&new_trans) < 0) {
      mailbox_set_index_error(box);
      return -1;
    }
  }
  return 0;
}
int rbox_mailbox_open(struct mailbox *box) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  struct sdbox_index_header hdr;
  bool need_resize;

  if (rbox_mailbox_alloc_index(mbox) < 0)
    return -1;

  if (rbox_open_mailbox(box) < 0) {
    return -1;
  }

  if (box->creating) {
    /* wait for mailbox creation to initialize the index */
    return 0;
  }

  /* get/generate mailbox guid */
  if (rbox_read_header(mbox, &hdr, FALSE, &need_resize) < 0) {
    /* looks like the mailbox is corrupted */
    (void)rbox_sync(mbox, RBOX_SYNC_FLAG_FORCE);
    if (rbox_read_header(mbox, &hdr, TRUE, &need_resize) < 0)
      i_zero(&hdr);
  }

  if (guid_128_is_empty(hdr.mailbox_guid)) {
    /* regenerate it */
    if (rbox_mailbox_create_indexes(box, NULL, NULL) < 0 || rbox_read_header(mbox, &hdr, TRUE, &need_resize) < 0) {
      return -1;
    }
  }

  memcpy(mbox->mailbox_guid, hdr.mailbox_guid, sizeof(mbox->mailbox_guid));

  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_open", NULL);
  FUNC_END();
  return 0;
}

void rbox_set_mailbox_corrupted(struct mailbox *box) {
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  struct sdbox_index_header hdr;
  bool need_resize;

  if (rbox_read_header(mbox, &hdr, TRUE, &need_resize) < 0 || hdr.rebuild_count == 0)
    mbox->corrupted_rebuild_count = 1;
  else
    mbox->corrupted_rebuild_count = hdr.rebuild_count;
}

static void rbox_mailbox_close(struct mailbox *box) {
  FUNC_START();
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
  struct expunged_item *const *moved_items, *moved_item;
  unsigned int moved_count, i;

  if (array_is_created(&rbox->moved_items)) {
    if (array_count(&rbox->moved_items) > 0) {
      moved_items = array_get(&rbox->moved_items, &moved_count);
      for (i = 0; i < moved_count; i++) {
        moved_item = moved_items[i];
        i_free(moved_item);
      }
      array_delete(&rbox->moved_items, array_count(&rbox->moved_items) - 1, 1);
    }
    array_free(&rbox->moved_items);
  }

  /*if (rbox->corrupted_rebuild_count != 0) {
    (void)rbox_sync(rbox);
  }*/
  index_storage_mailbox_close(box);
  FUNC_END();
}

static int dir_is_empty(struct mail_storage *storage, const char *path) {
  DIR *dir;
  struct dirent *d;
  int ret = 1;

  dir = opendir(path);
  if (dir == NULL) {
    if (errno == ENOENT) {
      /* race condition with DELETE/RENAME? */
      return 1;
    }
    mail_storage_set_critical(storage, "opendir(%s) failed: %m", path);
    return -1;
  }
  while ((d = readdir(dir)) != NULL) {
    if (*d->d_name == '.')
      continue;

    ret = 0;
    break;
  }
  if (closedir(dir) < 0) {
    mail_storage_set_critical(storage, "closedir(%s) failed: %m", path);
    ret = -1;
  }
  return ret;
}

int rbox_mailbox_create(struct mailbox *box, const struct mailbox_update *update, bool directory) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  const char *alt_path;
  struct stat st;
  int ret;

  if ((ret = index_storage_mailbox_create(box, directory)) <= 0) {
    debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_create (ret <= 0, 1)", NULL);
    FUNC_END_RET("index_storage_mailbox_create: ret <= 0");
    return ret;
  }

  if (mailbox_open(box) < 0) {
    FUNC_END_RET("mailbox_open: ret < 0");
    return -1;
  }

  if (mail_index_get_header(box->view)->uid_validity != 0) {
    mail_storage_set_error(box->storage, MAIL_ERROR_EXISTS, "Mailbox already exists");
    FUNC_END_RET("Mailbox already exists: ret == -1");
    return -1;
  }

  /* if alt path already exists and contains files, rebuild storage so
     that we don't start overwriting files. */
  ret = mailbox_get_path_to(box, MAILBOX_LIST_PATH_TYPE_ALT_MAILBOX, &alt_path);
  if (ret > 0 && stat(alt_path, &st) == 0) {
    ret = dir_is_empty(box->storage, alt_path);
    if (ret < 0)
      return -1;
    if (ret == 0) {
      mail_storage_set_critical(&storage->storage,
                                "Mailbox %s has existing files in alt path, "
                                "rebuilding storage to avoid losing messages",
                                box->vname);
      rbox_set_mailbox_corrupted(box);
      return -1;
    }
    /* dir is empty, ignore it */
  }

  i_debug("rbox_mailbox_create: mailbox update guid = %s",
          update != NULL ? guid_128_to_string(update->mailbox_guid) : "Invalid update");
  debug_print_rbox_mailbox(mbox, "rbox-storage::rbox_mailbox_create", NULL);
  FUNC_END();
  return rbox_mailbox_create_indexes(box, update, NULL);
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
                                             rbox_mailbox_close,
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
