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

#ifndef SRC_STORAGE_RBOX_RBOX_MAILBOX_LIST_FS_H_
#define SRC_STORAGE_RBOX_RBOX_MAILBOX_LIST_FS_H_

extern "C" {

#include "dovecot-all.h"
#include "debug-helper.h"
#include "guid.h"
#include "mailbox-list-fs.h"
#include "lib.h"
#include <sys/stat.h>
}

/* Assume that if atime < mtime, there are new mails. If it's good enough for
   UW-IMAP, it's good enough for us. */
#define STAT_GET_MARKED_FILE(st) \
  ((st).st_size == 0 ? MAILBOX_UNMARKED : (st).st_atime < (st).st_mtime ? MAILBOX_MARKED : MAILBOX_UNMARKED)

int rbox_fs_list_get_mailbox_flags(struct mailbox_list *list, const char *dir, const char *fname,
                                   enum mailbox_list_file_type type, enum mailbox_info_flags *flags_r);

#endif /* SRC_STORAGE_RBOX_RBOX_MAILBOX_LIST_FS_H_ */
