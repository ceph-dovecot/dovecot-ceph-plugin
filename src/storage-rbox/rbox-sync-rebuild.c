/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "lib.h"
#include "array.h"
#include "index-rebuild.h"
#include "mail-cache.h"
#include "rbox-storage.h"
#include "rbox-file.h"
#include "rbox-sync.h"
#include "debug-helper.h"

#include <dirent.h>

static void rbox_sync_set_uidvalidity(struct index_rebuild_context *ctx) {
  FUNC_START();
  uint32_t uid_validity;

  /* if uidvalidity is set in the old index, use it */
  uid_validity = mail_index_get_header(ctx->view)->uid_validity;
  if (uid_validity == 0)
    uid_validity = dbox_get_uidvalidity_next(ctx->box->list);

  mail_index_update_header(ctx->trans, offsetof(struct mail_index_header, uid_validity), &uid_validity,
                           sizeof(uid_validity), TRUE);
  i_debug("rbox_sync_set_uidvalidity: uid_validity = %u", uid_validity);
  rbox_dbg_print_mailbox(ctx->box, "rbox_sync_set_uidvalidity", NULL);
  FUNC_END();
}

static int rbox_sync_add_file_index(struct index_rebuild_context *ctx, struct dbox_file *file, uint32_t uid,
                                    bool primary) {
  FUNC_START();
  uint32_t seq;
  bool deleted;
  int ret;

  if ((ret = dbox_file_open(file, &deleted)) > 0) {
    if (deleted)
      return 0;
    ret = dbox_file_seek(file, 0);
  }
  if (ret == 0) {
    if ((ret = dbox_file_fix(file, 0)) > 0)
      ret = dbox_file_seek(file, 0);
  }

  if (ret <= 0) {
    if (ret < 0) {
      FUNC_END_RET("ret == 1; unfixable file");
      return -1;
    }

    i_warning("rbox: Skipping unfixable file: %s", file->cur_path);
    return 0;
  }

  if (!dbox_file_is_in_alt(file) && !primary) {
    /* we were supposed to open the file in alt storage, but it
       exists in primary storage as well. skip it to avoid adding
       it twice. */
    FUNC_END_RET("ret == 0; skip it to avoid adding it twice");
    return 0;
  }

  mail_index_append(ctx->trans, uid, &seq);
  T_BEGIN { index_rebuild_index_metadata(ctx, seq, uid); }
  T_END;
  FUNC_END();
  return 0;
}

static int rbox_sync_add_file(struct index_rebuild_context *ctx, const char *fname, bool primary) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)ctx->box;
  struct dbox_file *file;
  uint32_t uid;
  int ret;

  i_debug("rbox_sync_add_file: fname = %s, primary = %s", fname, btoa(primary));
  rbox_dbg_print_rbox_mailbox(mbox, "rbox_sync_add_file", NULL);

  if (strncmp(fname, RBOX_MAIL_FILE_PREFIX, strlen(RBOX_MAIL_FILE_PREFIX)) != 0) {
    FUNC_END_RET("ret == 0");
    return 0;
  }
  fname += strlen(RBOX_MAIL_FILE_PREFIX);

  if (str_to_uint32(fname, &uid) < 0 || uid == 0) {
    i_warning("rbox %s: Ignoring invalid filename %s", mailbox_get_path(ctx->box), fname);
    FUNC_END_RET("ret == 0");
    return 0;
  }

  file = rbox_file_init(mbox, uid);
  if (!primary)
    file->cur_path = file->alt_path;
  ret = rbox_sync_add_file_index(ctx, file, uid, primary);
  dbox_file_unref(&file);
  FUNC_END();
  return ret;
}

static int rbox_sync_index_rebuild_dir(struct index_rebuild_context *ctx, const char *path, bool primary) {
  FUNC_START();
  struct mail_storage *storage = ctx->box->storage;
  DIR *dir;
  struct dirent *d;
  int ret = 0;

  i_debug("rbox_sync_index_rebuild_dir: path = %s, primary = %s", path, btoa(primary));
  rbox_dbg_print_mailbox(ctx->box, "rbox_sync_index_rebuild_dir", NULL);

  dir = opendir(path);
  if (dir == NULL) {
    if (errno == ENOENT) {
      if (!primary) {
        /* alt directory doesn't exist, ignore */
        FUNC_END_RET("ret == 0; alt directory doesn't exist");
        return 0;
      }
      mailbox_set_deleted(ctx->box);
      FUNC_END_RET("ret == -1; primary directory doesn't exist");
      return -1;
    }
    mail_storage_set_critical(storage, "opendir(%s) failed: %m", path);
    FUNC_END_RET("ret == -1; opendir failed");
    return -1;
  }
  do {
    errno = 0;
    if ((d = readdir(dir)) == NULL)
      break;

    ret = rbox_sync_add_file(ctx, d->d_name, primary);
  } while (ret >= 0);
  if (errno != 0) {
    mail_storage_set_critical(storage, "readdir(%s) failed: %m", path);
    ret = -1;
  }

  if (closedir(dir) < 0) {
    mail_storage_set_critical(storage, "closedir(%s) failed: %m", path);
    ret = -1;
  }
  FUNC_END();
  return ret;
}

static void rbox_sync_update_header(struct index_rebuild_context *ctx) {
  FUNC_START();
  struct rbox_mailbox *mbox = (struct rbox_mailbox *)ctx->box;
  struct rbox_index_header hdr;
  bool need_resize;

  rbox_dbg_print_mailbox(ctx->box, "rbox_sync_update_header", NULL);

  if (rbox_read_header(mbox, &hdr, FALSE, &need_resize) < 0)
    i_zero(&hdr);
  if (guid_128_is_empty(hdr.mailbox_guid))
    guid_128_generate(hdr.mailbox_guid);
  if (++hdr.rebuild_count == 0)
    hdr.rebuild_count = 1;
  /* mailbox is being reset. this gets written directly there */
  mail_index_set_ext_init_data(ctx->box->index, mbox->hdr_ext_id, &hdr, sizeof(hdr));
  FUNC_END();
}

static int rbox_sync_index_rebuild_singles(struct index_rebuild_context *ctx) {
  FUNC_START();
  const char *path, *alt_path;
  int ret = 0;

  rbox_dbg_print_mailbox(ctx->box, "rbox_sync_index_rebuild_singles", NULL);

  path = mailbox_get_path(ctx->box);
  if (mailbox_get_path_to(ctx->box, MAILBOX_LIST_PATH_TYPE_ALT_MAILBOX, &alt_path) < 0) {
    FUNC_END_RET("ret == -1; mailbox_get_path_to failed");
    return -1;
  }

  rbox_sync_set_uidvalidity(ctx);
  if (rbox_sync_index_rebuild_dir(ctx, path, TRUE) < 0) {
    mail_storage_set_critical(ctx->box->storage, "rbox: Rebuilding failed on path %s", mailbox_get_path(ctx->box));
    ret = -1;
  } else if (alt_path != NULL) {
    if (rbox_sync_index_rebuild_dir(ctx, alt_path, FALSE) < 0) {
      mail_storage_set_critical(ctx->box->storage, "rbox: Rebuilding failed on alt path %s", alt_path);
      ret = -1;
    }
  }
  rbox_sync_update_header(ctx);
  FUNC_END();
  return ret;
}

int rbox_sync_index_rebuild(struct rbox_mailbox *mbox, bool force) {
  FUNC_START();
  struct index_rebuild_context *ctx;
  struct mail_index_view *view;
  struct mail_index_transaction *trans;
  struct rbox_index_header hdr;
  bool need_resize;
  int ret;

  i_debug("rbox_sync_index_rebuild: force = %s", btoa(force));
  rbox_dbg_print_rbox_mailbox(mbox, "rbox_sync_index_rebuild", NULL);

  if (!force && rbox_read_header(mbox, &hdr, FALSE, &need_resize) == 0) {
    if (hdr.rebuild_count != mbox->corrupted_rebuild_count && hdr.rebuild_count != 0) {
      /* already rebuilt by someone else */
      FUNC_END_RET("ret == 0; already rebuilt by someone else");
      return 0;
    }
  }
  i_warning("rbox %s: Rebuilding index", mailbox_get_path(&mbox->box));

  if (dbox_verify_alt_storage(mbox->box.list) < 0) {
    mail_storage_set_critical(mbox->box.storage,
                              "rbox %s: Alt storage not mounted, "
                              "aborting index rebuild",
                              mailbox_get_path(&mbox->box));
    FUNC_END_RET("ret == -1; Alt storage not mounted aborting index rebuild");
    return -1;
  }

  view = mail_index_view_open(mbox->box.index);
  trans = mail_index_transaction_begin(view, MAIL_INDEX_TRANSACTION_FLAG_EXTERNAL);

  ctx = index_index_rebuild_init(&mbox->box, view, trans);
  ret = rbox_sync_index_rebuild_singles(ctx);
  index_index_rebuild_deinit(&ctx, dbox_get_uidvalidity_next);

  if (ret < 0)
    mail_index_transaction_rollback(&trans);
  else {
    mail_index_unset_fscked(trans);
    ret = mail_index_transaction_commit(&trans);
  }
  mail_index_view_close(&view);
  mbox->corrupted_rebuild_count = 0;
  FUNC_END();
  return ret;
}
