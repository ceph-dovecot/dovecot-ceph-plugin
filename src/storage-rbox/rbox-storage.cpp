// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 * Copyright (c) 2007-2017 Dovecot authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
#include "rbox-storage.hpp"
#include <sys/stat.h>
#include <dirent.h>

#include <string>
#include <map>
#include <rados/librados.hpp>

extern "C" {

#include "dovecot-all.h"

#include "rbox-sync.h"
#include "debug-helper.h"
#include "guid.h"
#include "mailbox-list-fs.h"
#include "macros.h"
}

#include "rbox-mailbox-list-fs.h"

#include "../librmb/rados-cluster-impl.h"
#include "../librmb/rados-storage-impl.h"
#include "../librmb/rados-namespace-manager.h"
#include "../librmb/rados-dovecot-ceph-cfg-impl.h"
#include "../librmb/rados-guid-generator.h"
#include "../librmb/rados-metadata-storage-impl.h"

#include "rbox-copy.h"
#include "rbox-mail.h"

using std::string;

class RboxGuidGenerator : public librmb::RadosGuidGenerator {
 public:
  void generate_guid(std::string *guid_) override {
    guid_128_t namespace_guid;
    guid_128_generate(namespace_guid);
    *guid_ = guid_128_to_string(namespace_guid);
  }
};

extern struct mailbox rbox_mailbox;
extern struct mailbox_vfuncs rbox_mailbox_vfuncs;

struct mail_storage *rbox_storage_alloc(void) {
  FUNC_START();
  struct rbox_storage *r_storage;
  pool_t pool;
  pool = pool_alloconly_create("rbox storage", 256);
  r_storage = p_new(pool, struct rbox_storage, 1);
  i_zero(r_storage);
  r_storage->storage = rbox_storage;
  r_storage->storage.pool = pool;
  r_storage->cluster = new librmb::RadosClusterImpl();
  r_storage->s = new librmb::RadosStorageImpl(r_storage->cluster);
  r_storage->config = new librmb::RadosDovecotCephCfgImpl(&r_storage->s->get_io_ctx());
  r_storage->ns_mgr = new librmb::RadosNamespaceManager(r_storage->config);
  r_storage->ms = new librmb::RadosMetadataStorageImpl();
  r_storage->alt = new librmb::RadosStorageImpl(r_storage->cluster);

  // logfile is set when 90-plugin.conf param rados_save_cfg is evaluated.
  r_storage->save_log = new librmb::RadosSaveLog();

  FUNC_END();
  return &r_storage->storage;
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

  FUNC_END();
}

static const char *rbox_storage_find_root_dir(const struct mail_namespace *ns) {
  FUNC_START();
  const char *home;

  if (ns->owner != NULL && mail_user_get_home(ns->owner, &home) > 0) {
    const char *path = t_strconcat(home, "/rbox", NULL);
    if (access(path, R_OK | W_OK | X_OK) == 0) {
#ifdef DEBUG
      i_debug("rbox: root exists (%s)", path);
#endif
      FUNC_END();
      return path;
    }
#ifdef DEBUG
    i_debug("rbox: access(%s, rwx): failed: %m", path);
#endif
  }

  FUNC_END();
  return NULL;
}

bool rbox_storage_autodetect(const struct mail_namespace *ns, struct mailbox_list_settings *set) {
  FUNC_START();
  struct stat st;
  const char *path, *root_dir;

  if (set->root_dir != NULL) {
    root_dir = set->root_dir;
  } else {
    root_dir = rbox_storage_find_root_dir(ns);
    if (root_dir == NULL) {
#ifdef DEBUG
      i_debug("rbox: couldn't find root dir");
#endif

      FUNC_END();
      return FALSE;
    }
  }

  path = t_strconcat(root_dir, "/" RBOX_MAILBOX_DIR_NAME, NULL);
  if (stat(path, &st) < 0) {
#ifdef DEBUG
    i_debug("rbox autodetect: stat(%s) failed: %m", path);
#endif

    FUNC_END();
    return FALSE;
  }

  if (!S_ISDIR(st.st_mode)) {
#ifdef DEBUG
    i_debug("rbox autodetect: %s not a directory", path);
#endif

    FUNC_END();
    return FALSE;
  }

  set->root_dir = root_dir;
  rbox_storage_get_list_settings(ns, set);

  FUNC_END();
  return TRUE;
}

int rbox_storage_create(struct mail_storage *storage, struct mail_namespace *ns, const char **error_r) {
  FUNC_START();

  // RADOS initialization postponed to mailbox_open
  if (*ns->list->set.mailbox_dir_name == '\0') {
    *error_r = "rbox: MAILBOXDIR must not be empty";

    FUNC_END();
    return -1;
  }
  storage->unique_root_dir = p_strdup(storage->pool, ns->list->set.root_dir);

  FUNC_END();
  return 0;
}

void rbox_storage_destroy(struct mail_storage *storage) {
  FUNC_START();
  struct rbox_storage *r_storage = (struct rbox_storage *)storage;

  if (r_storage->s != nullptr) {
    r_storage->s->close_connection();
    delete r_storage->s;
    r_storage->s = nullptr;
  }
  if (r_storage->alt != nullptr) {
    r_storage->alt->close_connection();
    delete r_storage->alt;
    r_storage->alt = nullptr;
  }
  if (r_storage->cluster != nullptr) {
    r_storage->cluster->deinit();
    delete r_storage->cluster;
    r_storage->cluster = nullptr;
  }
  if (r_storage->ns_mgr != nullptr) {
    delete r_storage->ns_mgr;
    r_storage->ns_mgr = nullptr;
  }
  if (r_storage->config != nullptr) {
    delete r_storage->config;
    r_storage->config = nullptr;
  }
  if (r_storage->ms != nullptr) {
    delete r_storage->ms;
    r_storage->ms = nullptr;
  }
  if (r_storage->save_log != nullptr) {
    if (!r_storage->save_log->close()) {
      i_warning("unable to close save log file");
    }
    delete r_storage->save_log;
    r_storage->save_log = nullptr;
  }
  index_storage_destroy(storage);

  FUNC_END();
}

struct mailbox *rbox_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname,
                                   enum mailbox_flags flags) {
  FUNC_START();
  struct rbox_mailbox *rbox;

  /* rados can't work without index files */
  int intflags = flags & ~MAILBOX_FLAG_NO_INDEX_FILES;

  pool_t pool = pool_alloconly_create("rbox mailbox", 1024);
  rbox = p_new(pool, struct rbox_mailbox, 1);
  i_zero(rbox);
  rbox_mailbox.v = rbox_mailbox_vfuncs;
  rbox->box = rbox_mailbox;
  rbox->box.pool = pool;
  rbox->box.storage = storage;
  rbox->box.list = list;
  rbox->box.v = rbox_mailbox_vfuncs;
  rbox->box.mail_vfuncs = &rbox_mail_vfuncs;
  rbox->storage = (struct rbox_storage *)storage;

  index_storage_mailbox_alloc(&rbox->box, vname, static_cast<mailbox_flags>(intflags), MAIL_INDEX_PREFIX);

  struct index_mailbox_context *ibox = static_cast<index_mailbox_context *>(RBOX_INDEX_STORAGE_CONTEXT(&rbox->box));
  intflags = ibox->index_flags | MAIL_INDEX_OPEN_FLAG_KEEP_BACKUPS | MAIL_INDEX_OPEN_FLAG_NEVER_IN_MEMORY;
  ibox = RBOX_INDEX_STORAGE_CONTEXT(&rbox->box);
  ibox->index_flags = static_cast<mail_index_open_flags>(intflags);

  read_plugin_configuration(&rbox->box);
  // TODO(jrse): load dovecot config and eval is_ceph_posix_bugfix_enabled
  // cephfs does not support 2 hardlinks.
  if (rbox->storage->config->is_ceph_posix_bugfix_enabled()) {
    list->ns->list->v.get_mailbox_flags = rbox_fs_list_get_mailbox_flags;
  }

  FUNC_END();
  return &rbox->box;
}

static int rbox_mailbox_alloc_index(struct rbox_mailbox *rbox) {
  FUNC_START();

  if (index_storage_mailbox_alloc_index(&rbox->box) < 0)
    return -1;

  rbox->hdr_ext_id = mail_index_ext_register(rbox->box.index, "dbox-hdr", sizeof(struct rbox_index_header), 0, 0);
  /* set the initialization data in case the mailbox is created */
  struct rbox_index_header hdr;
  i_zero(&hdr);
  guid_128_generate(hdr.mailbox_guid);
  mail_index_set_ext_init_data(rbox->box.index, rbox->hdr_ext_id, &hdr, sizeof(hdr));

  memcpy(rbox->mailbox_guid, hdr.mailbox_guid, sizeof(rbox->mailbox_guid));

  // register index record holding the mail guid
  rbox->ext_id = mail_index_ext_register(rbox->box.index, "obox", 0, sizeof(struct obox_mail_index_record), 1);

  FUNC_END();
  return 0;
}

int rbox_read_header(struct rbox_mailbox *rbox, struct rbox_index_header *hdr, bool log_error, bool *need_resize_r) {
  FUNC_START();
  struct mail_index_view *view;
  const void *data;
  size_t data_size;
  int ret = 0;

  i_assert(rbox->box.opened);

  view = mail_index_view_open(rbox->box.index);
  mail_index_get_header_ext(view, rbox->hdr_ext_id, &data, &data_size);
  if (data_size < SDBOX_INDEX_HEADER_MIN_SIZE && (!rbox->box.creating || data_size != 0)) {
    if (log_error) {
      mail_storage_set_critical(&rbox->storage->storage, "rbox %s: Invalid box header", mailbox_get_path(&rbox->box));
    }
    ret = -1;
  } else {
    memset(hdr, 0, sizeof(*hdr));
    memcpy(hdr, data, I_MIN(data_size, sizeof(*hdr)));
    if (guid_128_is_empty(hdr->mailbox_guid)) {
      ret = -1;
#ifdef DEBUG
      i_debug("mailbox guid is null");
#endif
    } else {
      /* data is valid. remember it in case mailbox
         is being reset */
      mail_index_set_ext_init_data(rbox->box.index, rbox->hdr_ext_id, hdr, sizeof(*hdr));
    }
  }
  mail_index_view_close(&view);
  *need_resize_r = data_size < sizeof(*hdr);

  FUNC_END();
  return ret;
}

static int rbox_open_mailbox(struct mailbox *box) {
  FUNC_START();

  const char *box_path = mailbox_get_path(box);
  struct stat st;
  int ret = -1;

#ifdef HAVE_ITER_FROM_INDEX_DIR
  if (box->list->set.iter_from_index_dir) {
    /* Just because the index directory exists, it doesn't mean
           that the mailbox is selectable. Check that by seeing if
           dovecot.index.log exists. If it doesn't, fallback to
           checking for the dbox-Mails in the mail root directory.
           So this also means that if a mailbox is \NoSelect, listing
           it will always do a stat() for rbox-Mails in the mail root
           directory. That's not ideal, but this makes the behavior
           safer and \NoSelect mailboxes are somewhat rare. */
    if (mailbox_get_path_to(box, MAILBOX_LIST_PATH_TYPE_INDEX, &box_path) < 0)
      return -1;
    i_assert(box_path != NULL);
    box_path = t_strconcat(box_path, "/", box->index_prefix, ".log", NULL);
    ret = stat(box_path, &st);
  }
#endif

  if (ret < 0) {
    ret = stat(box_path, &st);
  }
  if (ret == 0) {
    /* exists, open it */
  } else if (errno == ENOENT) {
    mail_storage_set_error(box->storage, MAIL_ERROR_NOTFOUND, T_MAIL_ERR_MAILBOX_NOT_FOUND(box->vname));

    FUNC_END_RET("ret == -1");
    return -1;
  } else if (errno == EACCES) {
    mail_storage_set_critical(box->storage, "%s", mail_error_eacces_msg("stat", box_path));

    FUNC_END_RET("ret == -1");
    return -1;
  } else {
    mail_storage_set_critical(box->storage, "stat(%s) failed: %m", box_path);

    FUNC_END_RET("ret == -1");
    return -1;
  }

  if (index_storage_mailbox_open(box, FALSE) < 0) {
    FUNC_END_RET("ret == -1");
    return -1;
  }
  mail_index_set_fsync_mode(
      box->index, box->storage->set->parsed_fsync_mode,
      static_cast<mail_index_fsync_mask>(MAIL_INDEX_FSYNC_MASK_APPENDS | MAIL_INDEX_FSYNC_MASK_EXPUNGES));

  struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
  if (!array_is_created(&rbox->moved_items)) {
    i_array_init(&rbox->moved_items, 32);
  }

  FUNC_END();
  return 0;
}

void read_plugin_configuration(struct mailbox *box) {
  FUNC_START();
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  if (!r_storage->config->is_config_valid()) {
    std::map<std::string, std::string> *map = r_storage->config->get_config();
    for (std::map<std::string, std::string>::iterator it = map->begin(); it != map->end(); ++it) {
      std::string setting = it->first;
      r_storage->config->update_metadata(setting, mail_user_plugin_getenv(r_storage->storage.user, setting.c_str()));
#ifdef DEBUG
      struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
      i_debug("reading plugin conf: %s=%s", setting.c_str(),
              mail_user_plugin_getenv(rbox->storage->storage.user, setting.c_str()));
#endif
    }
    r_storage->config->set_config_valid(true);
    r_storage->save_log->set_save_log_file(r_storage->config->get_rados_save_log_file());
    if (!r_storage->save_log->open() && !r_storage->config->get_rados_save_log_file().empty()) {
      i_warning("unable to open the rados save log file %s", r_storage->config->get_rados_save_log_file().c_str());
    }
  }

  FUNC_END();
}

bool is_alternate_storage_set(uint8_t flags) { return (flags & RBOX_INDEX_FLAG_ALT) != 0; }

bool is_alternate_pool_valid(struct mailbox *_box) {
  return _box->list->set.alt_dir != NULL && strlen(_box->list->set.alt_dir) > 0;
}

int rbox_open_rados_connection(struct mailbox *box, bool alt_storage) {
  FUNC_START();

  struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
  librmb::RadosStorage *rados_storage = rbox->storage->s;

  // initialize storage with plugin configuration
  read_plugin_configuration(box);

  /* open connection to primary and alternative storage */
  int ret = rados_storage->open_connection(rbox->storage->config->get_pool_name(),
                                           rbox->storage->config->get_rados_cluster_name(),
                                           rbox->storage->config->get_rados_username());

  if (alt_storage) {
    ret = rbox->storage->alt->open_connection(box->list->set.alt_dir, rbox->storage->config->get_rados_cluster_name(),
                                              rbox->storage->config->get_rados_username());
  }

  if (ret == 1) {
    // already connected nothing to do!
    FUNC_END();
#ifdef DEBUG
    i_debug("connection to rados already open");
#endif
    return 0;
  }
  if (ret < 0) {
    i_error(
        "Open rados connection. Error(%d,%s) (pool_name(%s), cluster_name(%s), rados_user_name(%s), "
        "alt_storage(%d), "
        "alt_dir(%s) )",
        ret, strerror(ret * -1), rbox->storage->config->get_pool_name().c_str(),
        rbox->storage->config->get_rados_cluster_name().c_str(), rbox->storage->config->get_rados_username().c_str(),
        alt_storage, box->list->set.alt_dir);
    return ret;
  }

  ret = rbox->storage->config->load_rados_config();
  if (ret == -ENOENT) {  // config does not exist.
    i_debug("Rados config does not exist, creating default config");
    ret = rbox->storage->config->save_default_rados_config();
  }
  if (ret < 0) {
    i_error("unable to read rados_config return value : %d", ret);
    return ret;
  }
  rbox->storage->ms->create_metadata_storage(&rbox->storage->s->get_io_ctx(), rbox->storage->config);

  std::string uid;
  if (box->list->ns->owner != nullptr) {
    uid = box->list->ns->owner->username;
    uid += rbox->storage->config->get_user_suffix();
  } else {
    uid = rbox->storage->config->get_public_namespace();
  }
  std::string ns;
  if (!rbox->storage->ns_mgr->lookup_key(uid, &ns)) {
    RboxGuidGenerator guid_generator;
    ret = rbox->storage->ns_mgr->add_namespace_entry(uid, &ns, &guid_generator) ? 0 : -1;
  }
  if (ret >= 0) {
    rados_storage->set_namespace(ns);
    if (alt_storage) {
      rbox->storage->alt->set_namespace(ns);
    }
  } else {
    i_error("error namespace not set: for uid %s error code is: %d", uid.c_str(), ret);
  }

  FUNC_END();
  return ret;
}

static void rbox_update_header(struct rbox_mailbox *rbox, struct mail_index_transaction *trans,
                               const struct mailbox_update *update) {
  FUNC_START();

  struct rbox_index_header hdr, new_hdr;
  bool need_resize;

  if (rbox_read_header(rbox, &hdr, TRUE, &need_resize) < 0) {
    memset(&hdr, 0, sizeof(hdr));
    need_resize = TRUE;
  }

  new_hdr = hdr;

  if (update != NULL && !guid_128_is_empty(update->mailbox_guid)) {
    memcpy(new_hdr.mailbox_guid, update->mailbox_guid, sizeof(new_hdr.mailbox_guid));
  } else if (guid_128_is_empty(new_hdr.mailbox_guid)) {
    guid_128_generate(new_hdr.mailbox_guid);
  }

  if (need_resize) {
    mail_index_ext_resize_hdr(trans, rbox->hdr_ext_id, sizeof(new_hdr));
  }
  if (memcmp(&hdr, &new_hdr, sizeof(hdr)) != 0) {
    mail_index_update_header_ext(trans, rbox->hdr_ext_id, 0, &new_hdr, sizeof(new_hdr));
  }
  memcpy(rbox->mailbox_guid, new_hdr.mailbox_guid, sizeof(rbox->mailbox_guid));

  FUNC_END();
}

uint32_t rbox_get_uidvalidity_next(struct mailbox_list *list) {
  FUNC_START();

  const char *path;
  path = mailbox_list_get_root_forced(list, MAILBOX_LIST_PATH_TYPE_CONTROL);
  path = t_strconcat(path, "/" RBOX_UIDVALIDITY_FILE_NAME, NULL);

  FUNC_END();
  return mailbox_uidvalidity_next(list, path);
}

int rbox_mailbox_create_indexes(struct mailbox *box, const struct mailbox_update *update,
                                struct mail_index_transaction *trans) {
  FUNC_START();

  const struct mail_index_header *hdr;
  uint32_t uid_validity, uid_next;
  struct mail_index_view *view;

  if (trans == NULL) {
    trans = mail_index_transaction_begin(box->view, static_cast<mail_index_transaction_flags>(0));
  }
  view = mail_index_view_open(box->index);
  hdr = mail_index_get_header(view);

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
    mail_index_update_header(trans, offsetof(struct mail_index_header, first_recent_uid), &update->min_first_recent_uid,
                             sizeof(update->min_first_recent_uid), FALSE);
  }
  if (update != NULL && update->min_highest_modseq != 0 &&
      mail_index_modseq_get_highest(view) < update->min_highest_modseq) {
    mail_index_modseq_enable(box->index);
    mail_index_update_highest_modseq(trans, update->min_highest_modseq);
  }
  mail_index_view_close(&view);

#ifdef DOVECOT_CEPH_PLUGINS_HAVE_INDEX_POP3_UIDL_H
  if (box->inbox_user && box->creating) {
    /* initialize pop3-uidl header when creating mailbox
       (not on mailbox_update()) */
    index_pop3_uidl_set_max_uid(box, trans, 0);
  }
#endif

  rbox_update_header((struct rbox_mailbox *)box, trans, update);
  if (trans != NULL) {
    if (mail_index_transaction_commit(&trans) < 0) {
      mailbox_set_index_error(box);

      FUNC_END_RET("ret == -1");
      return -1;
    }
  }

  FUNC_END();
  return 0;
}
int rbox_mailbox_open(struct mailbox *box) {
  FUNC_START();
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
  struct rbox_index_header hdr;
  bool need_resize;

  if (rbox_mailbox_alloc_index(rbox) < 0) {
    FUNC_END();
    return -1;
  }

  if (rbox_open_mailbox(box) < 0) {
    FUNC_END();
    return -1;
  }

  if (box->creating) {
    /* wait for mailbox creation to initialize the index */
    FUNC_END();
    return 0;
  }

  i_zero(&hdr);
  /* get/generate mailbox guid */
  if (rbox_read_header(rbox, &hdr, FALSE, &need_resize) < 0) {
    /* looks like the mailbox is corrupted */
    (void)rbox_sync(rbox, RBOX_SYNC_FLAG_FORCE);
    if (rbox_read_header(rbox, &hdr, TRUE, &need_resize) < 0)
      i_zero(&hdr);
  }

  if (guid_128_is_empty(hdr.mailbox_guid)) {
    /* regenerate it */
    if (rbox_mailbox_create_indexes(box, NULL, NULL) < 0 || rbox_read_header(rbox, &hdr, TRUE, &need_resize) < 0) {
      return -1;
    }
  }

  memcpy(rbox->mailbox_guid, hdr.mailbox_guid, sizeof(rbox->mailbox_guid));

  FUNC_END();
  return 0;
}

void rbox_set_mailbox_corrupted(struct mailbox *box) {
  FUNC_START();

  struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
  bool need_resize;
  struct rbox_index_header hdr;
  i_zero(&hdr);

  if (rbox_read_header(rbox, &hdr, TRUE, &need_resize) < 0 || hdr.rebuild_count == 0)
    rbox->storage->corrupted_rebuild_count = 1;
  else
    rbox->storage->corrupted_rebuild_count = hdr.rebuild_count;

  rbox->storage->corrupted = TRUE;
#ifdef DEBUG
  i_debug("setting currupted rebuild count to : %d", rbox->storage->corrupted_rebuild_count);
#endif

  FUNC_END();
}

static void rbox_mailbox_close(struct mailbox *box) {
  FUNC_START();
  struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
  struct expunged_item *const *moved_items, *moved_item;

  if (array_is_created(&rbox->moved_items)) {
    if (array_count(&rbox->moved_items) > 0) {
      unsigned int moved_count;
      moved_items = array_get(&rbox->moved_items, &moved_count);
      for (unsigned int i = 0; i < moved_count; i++) {
        moved_item = moved_items[i];
        i_free(moved_item);
      }
      array_delete(&rbox->moved_items, array_count(&rbox->moved_items) - 1, 1);
    }
    array_free(&rbox->moved_items);
  }

  if (rbox->storage->corrupted_rebuild_count != 0) {
#ifdef DEBUG
    i_debug("storage corrupted rebuild count != 0 calling sync");
#endif
    (void)rbox_sync(rbox, static_cast<enum rbox_sync_flags>(0));
  }

  index_storage_mailbox_close(box);
  FUNC_END();
}

static int dir_is_empty(struct mail_storage *storage, const char *path) {
  FUNC_START();

  DIR *dir;
  struct dirent *d;
  int ret = 1;

  dir = opendir(path);
  if (dir == NULL) {
    if (errno == ENOENT) {
      /* race condition with DELETE/RENAME? */
      FUNC_END();
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

  FUNC_END();
  return ret;
}

int rbox_mailbox_create(struct mailbox *box, const struct mailbox_update *update, bool directory) {
  FUNC_START();
  const char *alt_path;
  struct stat st;
  int ret;
  struct mail_index_sync_ctx *sync_ctx;
  struct mail_index_view *view;
  struct mail_index_transaction *trans;
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;

  if ((ret = index_storage_mailbox_create(box, directory)) <= 0) {
    FUNC_END_RET("index_storage_mailbox_create: ret <= 0");
    return ret;
  }
  if (mailbox_open(box) < 0) {
    FUNC_END_RET("mailbox_open: ret < 0");
    return -1;
  }

  if (mail_index_get_header(box->view)->uid_validity != 0) {
    mail_storage_set_error(box->storage, MAIL_ERROR_EXISTS, "Mailbox already exists");
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
      struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
      mail_storage_set_critical(&r_storage->storage,
                                "Mailbox %s has existing files in alt path, "
                                "rebuilding storage to avoid losing messages",
                                box->vname);
      rbox_set_mailbox_corrupted(box);
      return -1;
    }
    /* dir is empty, ignore it */
  }
#ifdef DEBUG
  i_debug("rbox_mailbox_create: mailbox update guid = %s",
          update != NULL ? guid_128_to_string(update->mailbox_guid) : "Invalid update");
#endif

  /* use syncing as a lock */
  ret = mail_index_sync_begin(box->index, &sync_ctx, &view, &trans, 0);
  if (ret <= 0) {
    i_assert(ret != 0);
    mailbox_set_index_error(box);
    return -1;
  }
  if (mail_index_get_header(view)->uid_validity == 0) {
    if (rbox_mailbox_create_indexes(box, update, NULL) < 0) {
      mail_index_sync_rollback(&sync_ctx);
      return -1;
    }
  }

  FUNC_END();
  return mail_index_sync_commit(&sync_ctx);
}

static int rbox_mailbox_update(struct mailbox *box, const struct mailbox_update *update) {
  FUNC_START();

  if (!box->opened) {
    if (mailbox_open(box) < 0) {
      return -1;
    }
  }

  // TODO(peter): if (sdbox_mailbox_create_indexes(box, update, NULL) < 0) return -1;
  FUNC_END();
  return index_storage_mailbox_update(box, update);
}

int rbox_mailbox_get_metadata(struct mailbox *box, enum mailbox_metadata_items items,
                              struct mailbox_metadata *metadata_r) {
  FUNC_START();

  if (index_mailbox_get_metadata(box, items, metadata_r) < 0) {
    FUNC_END_RET("ret == -1");
    return -1;
  }

  if ((items & MAILBOX_METADATA_GUID) != 0) {
    struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
    memcpy(metadata_r->guid, rbox->mailbox_guid, sizeof(metadata_r->guid));
  }

#ifdef DEBUG
  if (metadata_r != NULL && metadata_r->cache_fields != NULL) {
    i_debug("metadata size = %lu", metadata_r->cache_fields->arr.element_size);
  }
#endif

  FUNC_END();
  return 0;
}

void rbox_notify_changes(struct mailbox *box) {
  FUNC_START();

  if (box->notify_callback == NULL) {
    mailbox_watch_remove_all(box);
  } else {
    mailbox_watch_add(box, mailbox_get_path(box));
  }

  FUNC_END();
}

int check_users_mailbox_delete_ns_object(struct mail_user *user, librmb::RadosDovecotCephCfg *config,
                                         librmb::RadosNamespaceManager *ns_mgr, librmb::RadosStorage *storage) {
  FUNC_START();
  int ret = 0;
  struct mail_namespace *ns = mail_namespace_find_inbox(user->namespaces);
  for (; ns != NULL; ns = ns->next) {
    struct mailbox_list_iterate_context *iter;
    const struct mailbox_info *info;
    iter = mailbox_list_iter_init(ns->list, "*", static_cast<enum mailbox_list_iter_flags>(
                                                     MAILBOX_LIST_ITER_RAW_LIST | MAILBOX_LIST_ITER_RETURN_NO_FLAGS));

    int total_mails = 0;
    while ((info = mailbox_list_iter_next(iter)) != NULL) {
      if ((info->flags & (MAILBOX_NONEXISTENT | MAILBOX_NOSELECT)) == 0) {
        struct mailbox *box_ = mailbox_alloc(ns->list, info->vname, MAILBOX_FLAG_IGNORE_ACLS);
        struct mailbox_status status;
        if (mailbox_get_status(box_, STATUS_MESSAGES, &status) < 0) {
          i_error("cannot get status of %s", info->vname);
          ++total_mails;  // make sure we do not delete anything due to invalid status query!!!
        } else {
#ifdef DEBUG
          i_debug("mailbox %s, has %d messages", info->vname, status.messages);
#endif
          total_mails += status.messages;
        }
        mailbox_free(&box_);
      }
    }
    if (mailbox_list_iter_deinit(&iter) < 0)
      ret = -1;

    if (total_mails == 0) {
      std::string uid = ns->owner->username;
      uid += config->get_user_suffix();
      std::string ns_str;
      ns_mgr->lookup_key(uid, &ns_str);
#ifdef DEBUG
      i_debug(
          "total number of mails in all mailboxes is  %d, deleting indirect namespace object for user %s with "
          "namespace: %s ",
          total_mails, uid.c_str(), ns_str.c_str());
#endif
      storage->set_namespace(config->get_user_ns());
      ret = storage->delete_mail(uid);
      if (ret < 0) {
        if (ret == -ENOENT) {
#ifdef DEBUG
          i_debug("indirect ns object(%s) already deleted error(%d), namespace(%s)", uid.c_str(), ret,
                  storage->get_namespace().c_str());
#endif
        } else {
          i_error("Error deleting ns object(%s) error(%d), namespace(%s)", uid.c_str(), ret,
                  storage->get_namespace().c_str());
        }
      }
    }
  }

  FUNC_END();
  return ret;
}

int rbox_storage_mailbox_delete(struct mailbox *box) {
  FUNC_START();
  int ret = index_storage_mailbox_delete(box);
  if (ret < 0) {
    i_error("while processing index_storage_mailbox_delete: %d", ret);
    return ret;
  }
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
  // 90 plugin konfigurierbar!
  read_plugin_configuration(box);
  if (!r_storage->config->is_rbox_check_empty_mailboxes()) {
    return ret;
  }

  ret = rbox_open_rados_connection(box, false);
  if (ret < 0) {
    i_error("rbox_storage_mailbox_delete: Opening rados connection : %d", ret);
    return ret;
  }
  if (r_storage->config->is_user_mapping()) {  //

    struct rbox_mailbox *rbox = (struct rbox_mailbox *)box;
    ret = check_users_mailbox_delete_ns_object(rbox->storage->storage.user, r_storage->config, r_storage->ns_mgr,
                                               r_storage->s);
  }

  FUNC_END();
  return ret;
}

struct mailbox_vfuncs rbox_mailbox_vfuncs = {index_storage_is_readonly,
                                             index_storage_mailbox_enable,
                                             index_storage_mailbox_exists,
                                             rbox_mailbox_open,
                                             rbox_mailbox_close,
                                             index_storage_mailbox_free,
                                             rbox_mailbox_create,
                                             rbox_mailbox_update,
                                             rbox_storage_mailbox_delete,
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
