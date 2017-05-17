
/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */
/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

extern "C" {

#include "lib.h"
#include "typeof-def.h"
#include "eacces-error.h"
#include "fdatasync-path.h"
#include "mkdir-parents.h"
#include "istream.h"
#include "ostream.h"
#include "str.h"
#include "fs-api.h"
#include "dbox-attachment.h"
#include "rbox-storage.h"
#include "rbox-file.h"
#include "debug-helper.h"

#include <stdio.h>
#include <utime.h>

}

#include "rbox-storage-struct.h"

static void rbox_file_init_paths(struct rbox_file *file, const char *fname) {
  FUNC_START();
  struct mailbox *box = &file->mbox->box;
  const char *alt_path;

  i_free(file->file.primary_path);
  i_free(file->file.alt_path);
  file->file.primary_path = i_strdup_printf("%s/%s", mailbox_get_path(box), fname);
  file->file.cur_path = file->file.primary_path;

  if (mailbox_get_path_to(box, MAILBOX_LIST_PATH_TYPE_ALT_MAILBOX, &alt_path) > 0)
    file->file.alt_path = i_strdup_printf("%s/%s", alt_path, fname);
  rbox_dbg_print_rbox_file(file, "rbox_file_init_paths", NULL);
  FUNC_END();
}

struct dbox_file *rbox_file_init(struct rbox_mailbox *mbox, uint32_t uid) {
  FUNC_START();
  struct rbox_file *file;
  const char *fname;

  file = i_new(struct rbox_file, 1);
  file->file.storage = &mbox->storage->storage;
  file->mbox = mbox;
  T_BEGIN {
    if (uid != 0) {
      fname = t_strdup_printf(RBOX_MAIL_FILE_FORMAT, uid);
      rbox_file_init_paths(file, fname);
      file->uid = uid;
    } else {
      rbox_file_init_paths(file, dbox_generate_tmp_filename());
    }
  }
  T_END;
  dbox_file_init(&file->file);

  i_debug("rbox_file_init: uid = %u", uid);
  rbox_dbg_print_rbox_file(file, "rbox_file_init", NULL);
  FUNC_END();
  return &file->file;
}

struct dbox_file *rbox_file_create(struct rbox_mailbox *mbox) {
  FUNC_START();
  struct dbox_file *file;

  file = rbox_file_init(mbox, 0);
  file->fd = file->storage->v.file_create_fd(file, file->primary_path, FALSE);

  rbox_dbg_print_dbox_file(file, "rbox_file_create", NULL);
  FUNC_END();
  return file;
}

void rbox_file_free(struct dbox_file *file) {
  FUNC_START();
  struct rbox_file *sfile = (struct rbox_file *)file;
  rbox_dbg_print_rbox_file(sfile, "rbox_file_free", NULL);

  if (sfile->attachment_pool != NULL)
    pool_unref(&sfile->attachment_pool);
  dbox_file_free(file);
  FUNC_END();
}

int rbox_file_get_attachments(struct dbox_file *file, const char **extrefs_r) {
  FUNC_START();
  const char *line;
  bool deleted;
  int ret;

  *extrefs_r = NULL;
  rbox_dbg_print_dbox_file(file, "rbox_file_get_attachments", NULL);

  /* read the metadata */
  ret = dbox_file_open(file, &deleted);
  if (ret > 0) {
    if (deleted) {
      FUNC_END_RET("ret == 0; deleted");
      return 0;
    }
    if ((ret = dbox_file_seek(file, 0)) > 0)
      ret = dbox_file_metadata_read(file);
  }
  if (ret <= 0) {
    if (ret < 0) {
      FUNC_END_RET("ret == -1; file_open failed");
      return -1;
    }
    /* corrupted file. we're deleting it anyway. */
    line = NULL;
  } else {
    line = dbox_file_metadata_get(file, DBOX_METADATA_EXT_REF);
  }
  if (line == NULL) {
    /* no attachments */
    FUNC_END_RET("ret == 0; no attachments");
    return 0;
  }
  *extrefs_r = line;
  FUNC_END();
  return 1;
}

const char *rbox_file_attachment_relpath(struct rbox_file *file, const char *srcpath) {
  FUNC_START();
  const char *p;

  p = strchr(srcpath, '-');
  if (p == NULL) {
    mail_storage_set_critical(file->mbox->box.storage, "rbox attachment path in invalid format: %s", srcpath);
  } else {
    p = strchr(p + 1, '-');
  }
  const char *relpath = t_strdup_printf("%s-%s-%u", p == NULL ? srcpath : t_strdup_until(srcpath, p),
                                        guid_128_to_string(file->mbox->mailbox_guid), file->uid);

  i_debug("rbox_file_attachment_relpath: srcpath = %s, relpath = %s", srcpath, relpath);
  rbox_dbg_print_rbox_file(file, "rbox_file_attachment_relpath", NULL);
  FUNC_END();
  return relpath;
}

static int rbox_file_rename_attachments(struct rbox_file *file) {
  FUNC_START();
  struct dbox_storage *storage = file->file.storage;
  struct fs_file *src_file, *dest_file;
  const char *const *pathp, *src, *dest;
  int ret = 0;

  array_foreach(&file->attachment_paths, pathp) T_BEGIN {
    src = t_strdup_printf("%s/%s", storage->attachment_dir, *pathp);
    dest = t_strdup_printf("%s/%s", storage->attachment_dir, rbox_file_attachment_relpath(file, *pathp));
    src_file = fs_file_init(storage->attachment_fs, src, FS_OPEN_MODE_READONLY);
    dest_file = fs_file_init(storage->attachment_fs, dest, FS_OPEN_MODE_READONLY);
    if (fs_rename(src_file, dest_file) < 0) {
      mail_storage_set_critical(&storage->storage, "%s", fs_last_error(storage->attachment_fs));
      ret = -1;
    }
    fs_file_deinit(&src_file);
    fs_file_deinit(&dest_file);
  }
  T_END;
  return ret;
}

int rbox_file_assign_uid(struct rbox_file *file, uint32_t uid, bool ignore_if_exists) {
  const char *p, *old_path, *dir, *new_fname, *new_path;
  struct stat st;

  i_assert(file->uid == 0);
  i_assert(uid != 0);

  old_path = file->file.cur_path;
  p = strrchr(old_path, '/');
  i_assert(p != NULL);
  dir = t_strdup_until(old_path, p);

  new_fname = t_strdup_printf(RBOX_MAIL_FILE_FORMAT, uid);
  new_path = t_strdup_printf("%s/%s", dir, new_fname);

  i_debug("rbox_file_assign_uid: uid = %u, ignore_if_exists = %s", uid, btoa(ignore_if_exists));
  i_debug("rbox_file_assign_uid: new_fname = %s, new_path = %s", new_fname, new_path);
  rbox_dbg_print_rbox_file(file, "", NULL);

  if (!ignore_if_exists && stat(new_path, &st) == 0) {
    mail_storage_set_critical(&file->file.storage->storage, "rbox: %s already exists, rebuilding index", new_path);
    rbox_set_mailbox_corrupted(&file->mbox->box);
    FUNC_END_RET("ret == -1; alredy exists");
    return -1;
  }
  if (rename(old_path, new_path) < 0) {
    mail_storage_set_critical(&file->file.storage->storage, "rename(%s, %s) failed: %m", old_path, new_path);
    FUNC_END_RET("ret == -1, rename failed");
    return -1;
  }
  rbox_file_init_paths(file, new_fname);
  file->uid = uid;

  if (array_is_created(&file->attachment_paths)) {
    if (rbox_file_rename_attachments(file) < 0)
      return -1;
  }
  FUNC_END();
  return 0;
}

static int rbox_file_unlink_aborted_save_attachments(struct rbox_file *file) {
  FUNC_START();
  struct dbox_storage *storage = file->file.storage;
  struct fs *fs = storage->attachment_fs;
  struct fs_file *fs_file;
  const char *const *pathp, *path;
  int ret = 0;

  array_foreach(&file->attachment_paths, pathp) T_BEGIN {
    /* we don't know if we aborted before renaming this attachment,
       so try deleting both source and dest path. the source paths
       point to temporary files (not to source messages'
       attachment paths), so it's safe to delete them. */
    path = t_strdup_printf("%s/%s", storage->attachment_dir, *pathp);
    i_debug("rbox_file_unlink_aborted_save_attachments: path = %s", path);
    fs_file = fs_file_init(fs, path, FS_OPEN_MODE_READONLY);
    if (fs_delete(fs_file) < 0 && errno != ENOENT) {
      mail_storage_set_critical(&storage->storage, "%s", fs_last_error(fs));
      ret = -1;
    }
    fs_file_deinit(&fs_file);

    path = t_strdup_printf("%s/%s", storage->attachment_dir, rbox_file_attachment_relpath(file, *pathp));
    fs_file = fs_file_init(fs, path, FS_OPEN_MODE_READONLY);
    if (fs_delete(fs_file) < 0 && errno != ENOENT) {
      mail_storage_set_critical(&storage->storage, "%s", fs_last_error(fs));
      ret = -1;
    }
    fs_file_deinit(&fs_file);
  }
  T_END;

  rbox_dbg_print_rbox_file(file, "rbox_file_unlink_aborted_save_attachments", NULL);
  FUNC_END();
  return ret;
}

int rbox_file_unlink_aborted_save(struct rbox_file *file) {
  FUNC_START();
  int ret = 0;

  if (unlink(file->file.cur_path) < 0) {
    mail_storage_set_critical(file->mbox->box.storage, "unlink(%s) failed: %m", file->file.cur_path);
    FUNC_END_RET("ret == -1; unlink failed");
    ret = -1;
  }
  if (array_is_created(&file->attachment_paths)) {
    if (rbox_file_unlink_aborted_save_attachments(file) < 0)
      ret = -1;
  }
  rbox_dbg_print_rbox_file(file, "rbox_file_unlink_aborted_save", NULL);
  FUNC_END();
  return ret;
}

int rbox_file_create_fd(struct dbox_file *file, const char *path, bool parents) {
  FUNC_START();
  struct rbox_file *sfile = (struct rbox_file *)file;
  struct mailbox *box = &sfile->mbox->box;
  const struct mailbox_permissions *perm = mailbox_get_permissions(box);
  const char *p, *dir;
  mode_t old_mask;
  int fd;

  i_debug("rbox_file_create_fd: path = %s, parents = %s", path, btoa(parents));
  rbox_dbg_print_dbox_file(file, "rbox_file_create_fd", NULL);

  old_mask = umask(0666 & ~perm->file_create_mode);
  fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
  umask(old_mask);
  if (fd == -1 && errno == ENOENT && parents && (p = strrchr(path, '/')) != NULL) {
    dir = t_strdup_until(path, p);
    if (mkdir_parents_chgrp(dir, perm->dir_create_mode, perm->file_create_gid, perm->file_create_gid_origin) < 0 &&
        errno != EEXIST) {
      mail_storage_set_critical(box->storage, "mkdir_parents(%s) failed: %m", dir);
      FUNC_END_RET("ret == -1; mkdir_parents_chgrp failed");
      return -1;
    }
    /* try again */
    old_mask = umask(0666 & ~perm->file_create_mode);
    fd = open(path, O_RDWR | O_CREAT | O_TRUNC, 0666);
    umask(old_mask);
  }
  if (fd == -1) {
    mail_storage_set_critical(box->storage, "open(%s, O_CREAT) failed: %m", path);
  } else if (perm->file_create_gid == (gid_t)-1) {
    /* no group change */
  } else if (fchown(fd, (uid_t)-1, perm->file_create_gid) < 0) {
    if (errno == EPERM) {
      mail_storage_set_critical(box->storage, "%s", eperm_error_get_chgrp("fchown", path, perm->file_create_gid,
                                                                          perm->file_create_gid_origin));
    } else {
      mail_storage_set_critical(box->storage, "fchown(%s, -1, %ld) failed: %m", path, (long)perm->file_create_gid);
    }
    /* continue anyway */
  }
  FUNC_END();
  return fd;
}

int rbox_file_move(struct dbox_file *file, bool alt_path) {
  FUNC_START();
  struct mail_storage *storage = &file->storage->storage;
  struct ostream *output;
  const char *dest_dir, *temp_path, *dest_path, *p;
  struct stat st;
  struct utimbuf ut;
  bool deleted;
  int out_fd, ret = 0;

  i_debug("rbox_file_move: alt_path = %s", btoa(alt_path));
  rbox_dbg_print_dbox_file(file, "rbox_file_move", NULL);

  i_assert(file->input != NULL);

  if (dbox_file_is_in_alt(file) == alt_path) {
    FUNC_END_RET("ret == 0; nothing to do");
    return 0;
  }
  if (file->alt_path == NULL) {
    FUNC_END_RET("ret == 0; file->alt_path == NULL");
    return 0;
  }

  if (stat(file->cur_path, &st) < 0 && errno == ENOENT) {
    /* already expunged/moved by another session */
    FUNC_END_RET("ret == 0; already expunged/moved by another session");
    return 0;
  }

  dest_path = !alt_path ? file->primary_path : file->alt_path;

  i_assert(dest_path != NULL);

  p = strrchr(dest_path, '/');
  i_assert(p != NULL);
  dest_dir = t_strdup_until(dest_path, p);
  temp_path = t_strdup_printf("%s/%s", dest_dir, dbox_generate_tmp_filename());

  /* first copy the file. make sure to catch every possible error
     since we really don't want to break the file. */
  out_fd = file->storage->v.file_create_fd(file, temp_path, TRUE);
  if (out_fd == -1) {
    FUNC_END_RET("ret == -1; create_fd failed");
    return -1;
  }

  output = o_stream_create_fd_file(out_fd, 0, FALSE);
  i_stream_seek(file->input, 0);
  ret = o_stream_send_istream(output, file->input) > 0 ? 0 : -1;
  if (o_stream_nfinish(output) < 0) {
    mail_storage_set_critical(storage, "write(%s) failed: %s", temp_path, o_stream_get_error(output));
    ret = -1;
  } else if (file->input->stream_errno != 0) {
    mail_storage_set_critical(storage, "read(%s) failed: %s", temp_path, i_stream_get_error(file->input));
    ret = -1;
  }
  o_stream_unref(&output);

  if (storage->set->parsed_fsync_mode != FSYNC_MODE_NEVER && ret == 0) {
    if (fsync(out_fd) < 0) {
      mail_storage_set_critical(storage, "fsync(%s) failed: %m", temp_path);
      ret = -1;
    }
  }
  if (close(out_fd) < 0) {
    mail_storage_set_critical(storage, "close(%s) failed: %m", temp_path);
    ret = -1;
  }
  if (ret < 0) {
    i_unlink(temp_path);
    FUNC_END_RET("ret == -1");
    return -1;
  }
  /* preserve the original atime/mtime. this isn't necessary for Dovecot,
     but could be useful for external reasons. */
  ut.actime = st.st_atime;
  ut.modtime = st.st_mtime;
  if (utime(temp_path, &ut) < 0) {
    mail_storage_set_critical(storage, "utime(%s) failed: %m", temp_path);
  }

  /* the temp file was successfully written. rename it now to the
     destination file. the destination shouldn't exist, but if it does
     its contents should be the same (except for maybe older metadata) */
  if (rename(temp_path, dest_path) < 0) {
    mail_storage_set_critical(storage, "rename(%s, %s) failed: %m", temp_path, dest_path);
    i_unlink_if_exists(temp_path);
    FUNC_END_RET("ret == -1; rename failed");
    return -1;
  }
  if (storage->set->parsed_fsync_mode != FSYNC_MODE_NEVER) {
    if (fdatasync_path(dest_dir) < 0) {
      mail_storage_set_critical(storage, "fdatasync(%s) failed: %m", dest_dir);
      i_unlink(dest_path);
      FUNC_END_RET("ret == -1; fdatasync_path failed");
      return -1;
    }
  }
  if (unlink(file->cur_path) < 0) {
    dbox_file_set_syscall_error(file, "unlink()");
    if (errno == EACCES) {
      /* configuration problem? revert the write */
      i_unlink(dest_path);
    }
    /* who knows what happened to the file. keep both just to be
       sure both won't get deleted. */
    FUNC_END_RET("ret == -1; unlink failed");
    return -1;
  }

  /* file was successfully moved - reopen it */
  dbox_file_close(file);
  if (dbox_file_open(file, &deleted) <= 0) {
    mail_storage_set_critical(storage, "dbox_file_move(%s): reopening file failed", dest_path);
    FUNC_END_RET("ret == -1; dbox_file_open failed");
    return -1;
  }
  FUNC_END();
  return 0;
}

static int rbox_unlink_attachments(struct rbox_file *sfile, const ARRAY_TYPE(mail_attachment_extref) * extrefs) {
  FUNC_START();
  struct dbox_storage *storage = sfile->file.storage;
  const struct mail_attachment_extref *extref;
  const char *path;
  int ret = 0;

  rbox_dbg_print_rbox_file(sfile, "rbox_unlink_attachments", NULL);

  array_foreach(extrefs, extref) T_BEGIN {
    path = rbox_file_attachment_relpath(sfile, extref->path);
    if (index_attachment_delete(&storage->storage, storage->attachment_fs, path) < 0)
      ret = -1;
  }
  T_END;
  FUNC_END();
  return ret;
}

int rbox_file_unlink_with_attachments(struct rbox_file *sfile) {
  FUNC_START();
  ARRAY_TYPE(mail_attachment_extref) extrefs;
  const char *extrefs_line;
  pool_t pool;
  int ret;

  ret = rbox_file_get_attachments(&sfile->file, &extrefs_line);
  if (ret < 0) {
    FUNC_END_RET("ret == -1; get_attachments failed");
    return -1;
  }
  if (ret == 0) {
    /* no attachments */
    FUNC_END_RET("ret = file_unlink; no attachments");
    return dbox_file_unlink(&sfile->file);
  }

  pool = pool_alloconly_create("rbox attachments unlink", 1024);
  p_array_init(&extrefs, pool, 16);
  if (!index_attachment_parse_extrefs(extrefs_line, pool, &extrefs)) {
    i_warning("%s: Ignoring corrupted extref: %s", sfile->file.cur_path, extrefs_line);
    array_clear(&extrefs);
  }

  /* try to delete the file first, so if it fails we don't have
     missing attachments */
  if ((ret = dbox_file_unlink(&sfile->file)) >= 0)
    (void)rbox_unlink_attachments(sfile, &extrefs);
  pool_unref(&pool);
  FUNC_END();
  return ret;
}
