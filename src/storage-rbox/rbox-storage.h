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
#include "rbox-storage-struct.h"
/**
 * @brief create rbox storage
 * @param[in] storage valid ptr to storage
 * @param[in] ns valid ptr to namespace
 * @param[out] error msg in case of error.
 *
 * @return linux error code or >=0 if sucessful
 */
extern int rbox_storage_create(struct mail_storage *storage, struct mail_namespace *ns, const char **error_r);
/*!
 * @brief allocate struct mail_storage
 * @return new mail_storage
 */
extern struct mail_storage *rbox_storage_alloc(void);
/**
 * @brief destroy mail_storage
 */
extern void rbox_storage_destroy(struct mail_storage *storage);
extern void rbox_storage_get_list_settings(const struct mail_namespace *ns, struct mailbox_list_settings *set);
extern bool rbox_storage_autodetect(const struct mail_namespace *ns, struct mailbox_list_settings *set);
extern struct mailbox *rbox_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname,
                                          enum mailbox_flags flags);

extern void rbox_set_mailbox_corrupted(struct mailbox *box);
extern bool is_alternate_storage_set(uint8_t flags);
extern bool is_alternate_pool_valid(struct mailbox *_box);
extern struct mail_storage rbox_storage;
/**
 * @brief entry function to create the rados connection
 * @param[in] box valid mailbox (state open)
 * @param[in] alt_storage indicates if alt_storage should be used.
 */
extern int rbox_open_rados_connection(struct mailbox *box, bool alt_storage);
/**
 * @brief reads the 90-plugin.conf section
 * @param[in] box mailbox (state open).
 */
extern void read_plugin_configuration(struct mailbox *box);
/**
 * @brief: deletes the given mailbox
 */
extern int rbox_storage_mailbox_delete(struct mailbox *box);

/**
 * check for flag
 *
 * @param box mailbox
 * @param ext_id header extension id
 * @param flags_offset flags offset
 * @param flag flag
 * @return
 */
extern bool rbox_header_have_flag(struct mailbox *box, uint32_t ext_id, unsigned int flags_offset, uint8_t flag);

#ifdef __cplusplus
}
#endif

/* Flag specifies if the message should be in primary or alternative storage */
#define RBOX_INDEX_FLAG_ALT MAIL_INDEX_MAIL_FLAG_BACKEND

#define SDBOX_INDEX_HEADER_MIN_SIZE (sizeof(uint32_t))

struct obox_mail_index_record {
  unsigned char guid[GUID_128_SIZE];
  unsigned char oid[GUID_128_SIZE];
};
/**
 * @brief: rbox mailbox structure
 */
struct rbox_mailbox {
  struct mailbox box;
  /** mailbox storage holding references to rados storage and configuration **/
  struct rbox_storage *storage;

  /** extended header id **/
  uint32_t hdr_ext_id;
  /** ext id **/
  uint32_t ext_id;
  /** unique identifier **/
  guid_128_t mailbox_guid;
};

enum rbox_index_header_flags {
  /* messages' metadata contain POP3 UIDLs */
  RBOX_INDEX_HEADER_FLAG_HAVE_POP3_UIDLS = 0x01,
  /* messages' metadata contain POP3 orders */
  RBOX_INDEX_HEADER_FLAG_HAVE_POP3_ORDERS = 0x02
};

#endif  // SRC_STORAGE_RBOX_RBOX_STORAGE_H_
