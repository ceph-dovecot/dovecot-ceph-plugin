// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_STORAGE_RBOX_RBOX_STORAGE_H_
#define SRC_STORAGE_RBOX_RBOX_STORAGE_H_

#define RBOX_STORAGE_NAME "rbox"

#ifdef __cplusplus
extern "C" {
#endif

#include "dovecot-all.h"

extern int rbox_storage_create(struct mail_storage *storage, struct mail_namespace *ns, const char **error_r);
extern struct mail_storage *rbox_storage_alloc(void);
extern void rbox_storage_destroy(struct mail_storage *storage);
extern void rbox_storage_get_list_settings(const struct mail_namespace *ns, struct mailbox_list_settings *set);
extern bool rbox_storage_autodetect(const struct mail_namespace *ns, struct mailbox_list_settings *set);
extern struct mailbox *rbox_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname,
                                          enum mailbox_flags flags);

extern void rbox_set_mailbox_corrupted(struct mailbox *box);
extern bool is_alternate_storage_set(uint8_t flags);
extern bool is_alternate_pool_valid(struct mailbox *_box);
extern struct mail_storage rbox_storage;
extern int rbox_open_rados_connection(struct mailbox *box, bool alt_storage);
extern int read_plugin_configuration(struct mailbox *box);
extern int rbox_storage_mailbox_delete(struct mailbox *box);

#ifdef __cplusplus
}
#endif

/* Flag specifies if the message should be in primary or alternative storage */
#define RBOX_INDEX_FLAG_ALT MAIL_INDEX_MAIL_FLAG_BACKEND

#define SDBOX_INDEX_HEADER_MIN_SIZE (sizeof(uint32_t))
struct sdbox_index_header {
  /* increased every time a full mailbox rebuild is done */
  uint32_t rebuild_count;
  guid_128_t mailbox_guid;
  uint8_t flags; /* enum dbox_index_header_flags */
  uint8_t unused[3];
};

struct obox_mail_index_record {
  unsigned char guid[GUID_128_SIZE];
  unsigned char oid[GUID_128_SIZE];
};

struct rbox_mailbox {
  struct mailbox box;
  struct rbox_storage *storage;

  uint32_t hdr_ext_id;
  uint32_t ext_id;

  guid_128_t mailbox_guid;

  ARRAY(struct expunged_item *) moved_items;
};

#endif  // SRC_STORAGE_RBOX_RBOX_STORAGE_H_
