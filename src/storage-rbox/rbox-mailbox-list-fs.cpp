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

#include "rbox-mailbox-list-fs.h"

// add some operators to handle this enum hell from dovecot
// code as defined in the C++ standard '17.5.2.1.3 Bitmask types'
inline constexpr mailbox_info_flags operator&(mailbox_info_flags X, mailbox_info_flags Y) {
  return static_cast<mailbox_info_flags>(static_cast<int>(X) & static_cast<int>(Y));
}
inline constexpr mailbox_info_flags operator|(mailbox_info_flags X, mailbox_info_flags Y) {
  return static_cast<mailbox_info_flags>(static_cast<int>(X) | static_cast<int>(Y));
}
inline constexpr mailbox_info_flags operator^(mailbox_info_flags X, mailbox_info_flags Y) {
  return static_cast<mailbox_info_flags>(static_cast<int>(X) ^ static_cast<int>(Y));
}
inline constexpr mailbox_info_flags operator~(mailbox_info_flags X) {
  return static_cast<mailbox_info_flags>(~static_cast<int>(X));
}
inline mailbox_info_flags &operator&=(mailbox_info_flags &X, mailbox_info_flags Y) {
  X = X & Y;
  return X;
}
inline mailbox_info_flags &operator|=(mailbox_info_flags &X, mailbox_info_flags Y) {
  X = X | Y;
  return X;
}
inline mailbox_info_flags &operator^=(mailbox_info_flags &X, mailbox_info_flags Y) {
  X = X ^ Y;
  return X;
}

static int rbox_list_is_maildir_mailbox(struct mailbox_list *list, const char *dir, const char *fname,
                                        enum mailbox_list_file_type type, enum mailbox_info_flags *flags_r) {
  FUNC_START();

  const char *path, *maildir_path;
  struct stat st, st2;
  bool mailbox_files;

  switch (type) {
    case MAILBOX_LIST_FILE_TYPE_FILE:
    case MAILBOX_LIST_FILE_TYPE_OTHER:
      /* non-directories aren't valid */
      *flags_r |= MAILBOX_NOSELECT | MAILBOX_NOINFERIORS;
      return 0;

    case MAILBOX_LIST_FILE_TYPE_DIR:
    case MAILBOX_LIST_FILE_TYPE_UNKNOWN:
    case MAILBOX_LIST_FILE_TYPE_SYMLINK:
      break;
    default:
      return -1;
  }

  path = t_strdup_printf("%s/%s", dir, fname);
  if (stat(path, &st) < 0) {
    if (errno == ENOENT) {
      *flags_r |= MAILBOX_NONEXISTENT;
      return 0;
    } else {
      /* non-selectable. probably either access denied, or
         symlink destination not found. don't bother logging
         errors. */
      *flags_r |= MAILBOX_NOSELECT;
      return 1;
    }
  }
  if (!S_ISDIR(st.st_mode)) {
    if (strncmp(fname, ".nfs", 4) == 0) {
      /* temporary NFS file */
      *flags_r |= MAILBOX_NONEXISTENT;
    } else {
      *flags_r |= MAILBOX_NOSELECT | MAILBOX_NOINFERIORS;
    }
    return 0;
  }

  /* ok, we've got a directory. see what we can do about it. */

  /* 1st link is "."
     2nd link is ".."
     3rd link is either child mailbox or mailbox dir
     rest of the links are child mailboxes

     if mailboxes are files, then 3+ links are all child mailboxes.
  */
  mailbox_files = (list->flags & MAILBOX_LIST_FLAG_MAILBOX_FILES) != 0;
  if (st.st_nlink == 2 && !mailbox_files) {
    *flags_r |= MAILBOX_NOSELECT;
    return 1;
  }

  /* we have at least one directory. see if this mailbox is selectable */
  maildir_path = t_strconcat(path, "/", list->set.maildir_name, NULL);
  if (stat(maildir_path, &st2) < 0)
    *flags_r |= MAILBOX_NOSELECT | MAILBOX_CHILDREN;
  else if (!S_ISDIR(st2.st_mode)) {
    if (mailbox_files) {
      *flags_r |= st.st_nlink == 2 ? MAILBOX_NOCHILDREN : MAILBOX_CHILDREN;
    } else {
      *flags_r |= MAILBOX_NOSELECT | MAILBOX_CHILDREN;
    }
  } else {
    /* now we know what link count 3 means. */
    if (st.st_nlink == 3) {
      *flags_r |= MAILBOX_NOCHILDREN;
    } else if (st.st_nlink < 2) {
      *flags_r |= MAILBOX_NOCHILDREN;
    } else
      *flags_r |= MAILBOX_CHILDREN;
  }
  *flags_r |= MAILBOX_SELECT;
  FUNC_END();
  return 1;
}
static bool rbox_is_inbox_file(struct mailbox_list *list, const char *path, const char *fname) {
  FUNC_START();

  const char *inbox_path;

  if (strcasecmp(fname, "INBOX") != 0) {
    FUNC_END();
    return FALSE;
  }

  if (mailbox_list_get_path(list, "INBOX", MAILBOX_LIST_PATH_TYPE_MAILBOX, &inbox_path) <= 0)
    i_unreached();

  FUNC_END();
  return strcmp(inbox_path, path) == 0;
}
int rbox_fs_list_get_mailbox_flags(struct mailbox_list *list, const char *dir, const char *fname,
                                   enum mailbox_list_file_type type, enum mailbox_info_flags *flags_r) {
  FUNC_START();

  struct stat st;
  const char *path;

  *flags_r = static_cast<mailbox_info_flags>(0);
#if DOVECOT_PREREQ(2, 3, 0)
  if (*list->set.maildir_name != '\0' && !list->set.iter_from_index_dir) {
#else
  if (*list->set.maildir_name != '\0') {
#endif
    /* maildir_name is set: This is the simple case that works for
       all mail storage formats, because the only thing that
       matters for existence or child checks is whether the
       maildir_name exists or not. For example with Maildir this
       doesn't care whether the "cur" directory exists; as long
       as the parent maildir_name exists, the Maildir is
       selectable. */
    return rbox_list_is_maildir_mailbox(list, dir, fname, type, flags_r);
  }
  /* maildir_name is not set: Now we (may) need to use storage-specific
     code to determine whether the mailbox is selectable or if it has
     children.

     We're here also when iterating from index directory, because even
     though maildir_name is set, it's not used for index directory.
  */
#if DOVECOT_PREREQ(2, 3, 0)
  if (!list->set.iter_from_index_dir &&
#else
  if (
#endif
      list->v.is_internal_name != NULL && list->v.is_internal_name(list, fname)) {
    /* skip internal dirs. For example Maildir's cur/new/tmp */
    *flags_r |= MAILBOX_NOSELECT;
    return 0;
  }

  switch (type) {
    case MAILBOX_LIST_FILE_TYPE_DIR:
      /* We know that we're looking at a directory. If the storage
         uses files, it has to be a \NoSelect directory. */
      if ((list->flags & MAILBOX_LIST_FLAG_MAILBOX_FILES) != 0) {
        *flags_r |= MAILBOX_NOSELECT;
        return 1;
      }
      break;
    case MAILBOX_LIST_FILE_TYPE_FILE:
      /* We know that we're looking at a file. If the storage
         doesn't use files, it's not a mailbox and we want to skip
         it. */
      if ((list->flags & MAILBOX_LIST_FLAG_MAILBOX_FILES) == 0) {
        *flags_r |= MAILBOX_NOSELECT | MAILBOX_NOINFERIORS;
        return 0;
      }
      break;
    case MAILBOX_LIST_FILE_TYPE_UNKNOWN:
    case MAILBOX_LIST_FILE_TYPE_SYMLINK:
    case MAILBOX_LIST_FILE_TYPE_OTHER:
    default:
      break;
  }

  /* we've done all filtering we can before stat()ing */
  path = t_strconcat(dir, "/", fname, NULL);
  if (stat(path, &st) < 0) {
    if (ENOTFOUND(errno)) {
      *flags_r |= MAILBOX_NONEXISTENT;
      return 0;
    } else if (ENOACCESS(errno)) {
      *flags_r |= MAILBOX_NOSELECT;
      return 1;
    } else {
      /* non-selectable. probably either access denied, or
         symlink destination not found. don't bother logging
         errors. */
      mailbox_list_set_critical(list, "stat(%s) failed: %m", path);
      return -1;
    }
  }

  if (!S_ISDIR(st.st_mode)) {
    if (strncmp(fname, ".nfs", 4) == 0) {
      /* temporary NFS file */
      *flags_r |= MAILBOX_NONEXISTENT;
      return 0;
    }

    if ((list->flags & MAILBOX_LIST_FLAG_MAILBOX_FILES) == 0) {
      *flags_r |= MAILBOX_NOSELECT | MAILBOX_NOINFERIORS;
      return 0;
    }
    /* looks like a valid mailbox file */
    if (rbox_is_inbox_file(list, path, fname) && strcmp(fname, "INBOX") != 0) {
      /* it's possible for INBOX to have child
         mailboxes as long as the inbox file itself
         isn't in <mail root>/INBOX */
    } else {
      *flags_r |= MAILBOX_NOINFERIORS;
    }
    /* Return mailbox files as always existing. The current
       mailbox_exists() code would do the same stat() anyway
       without further checks, so might as well avoid the second
       stat(). */
    *flags_r |= MAILBOX_SELECT;
    *flags_r |= STAT_GET_MARKED_FILE(st);
    return 1;
  }

  /* This is a directory */
  if ((list->flags & MAILBOX_LIST_FLAG_MAILBOX_FILES) != 0) {
    /* We should get here only if type is
       MAILBOX_LIST_FILE_TYPE_UNKNOWN because the filesystem didn't
       return the type. Normally this should have already been
       handled by the MAILBOX_LIST_FILE_TYPE_DIR check above. */
    *flags_r |= MAILBOX_NOSELECT;
    return 1;
  }

#if DOVECOT_PREREQ(2, 3, 0)
  if (list->v.is_internal_name == NULL || list->set.iter_from_index_dir) {
#else
  if (list->v.is_internal_name == NULL) {
#endif
    /* This mailbox format doesn't use any special directories
       (e.g. Maildir's cur/new/tmp). In that case we can look at
       the directory's link count to determine whether there are
       children or not. The directory's link count equals the
       number of subdirectories it has. The first two links are
       for "." and "..".

       link count < 2 can happen with filesystems that don't
       support link counts. we'll just ignore them for now.. */
    if (st.st_nlink == 2)
      *flags_r |= MAILBOX_NOCHILDREN;
    else if (st.st_nlink > 2)
      *flags_r |= MAILBOX_CHILDREN;
  }
  FUNC_END();
  return 1;
}
