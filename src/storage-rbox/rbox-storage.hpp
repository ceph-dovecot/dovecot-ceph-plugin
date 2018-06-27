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

#ifndef SRC_STORAGE_RBOX_RBOX_STORAGE_HPP_
#define SRC_STORAGE_RBOX_RBOX_STORAGE_HPP_

#include "rbox-storage.h"

#define RBOX_SUBSCRIPTION_FILE_NAME "subscriptions"
#define RBOX_UIDVALIDITY_FILE_NAME "dovecot-uidvalidity"
#define RBOX_TEMP_FILE_PREFIX ".temp."

#define RBOX_MAILBOX_DIR_NAME "mailboxes"
#define RBOX_TRASH_DIR_NAME "trash"
#define RBOX_MAILDIR_NAME "rbox-Mails"

#ifdef __cplusplus
#include "../librmb/rados-cluster-impl.h"
#include "../librmb/rados-storage-impl.h"
#include "../librmb/rados-namespace-manager.h"
#include "../librmb/rados-dovecot-ceph-cfg.h"
#include "../librmb/rados-metadata-storage-impl.h"
#include "../librmb/rados-save-log.h"

struct rbox_storage {
  struct mail_storage storage;

  librmb::RadosCluster *cluster;
  librmb::RadosStorage *s;
  librmb::RadosDovecotCephCfg *config;
  librmb::RadosNamespaceManager *ns_mgr;
  librmb::RadosMetadataStorage *ms;
  librmb::RadosStorage *alt;
  librmb::RadosSaveLog *save_log;
};

#endif

struct index_rebuild_context;
extern void rbox_sync_update_header(struct index_rebuild_context *ctx);
extern struct mail_vfuncs rbox_mail_vfuncs;
extern uint32_t rbox_get_uidvalidity_next(struct mailbox_list *list);
extern struct mail_save_context *rbox_save_alloc(struct mailbox_transaction_context *_t);
extern int rbox_save_begin(struct mail_save_context *ctx, struct istream *input);
extern int rbox_save_continue(struct mail_save_context *ctx);
extern int rbox_save_finish(struct mail_save_context *ctx);
extern void rbox_save_cancel(struct mail_save_context *ctx);
extern int rbox_transaction_save_commit_pre(struct mail_save_context *ctx);
extern void rbox_transaction_save_commit_post(struct mail_save_context *ctx,
                                              struct mail_index_transaction_commit_result *result);
extern void rbox_transaction_save_rollback(struct mail_save_context *ctx);

extern int rbox_mailbox_open(struct mailbox *box);
extern int rbox_mailbox_create(struct mailbox *box, const struct mailbox_update *update, bool directory);
extern int rbox_mailbox_get_metadata(struct mailbox *box, enum mailbox_metadata_items items,
                                     struct mailbox_metadata *metadata_r);

extern struct mailbox_sync_context *rbox_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags);
extern void rbox_notify_changes(struct mailbox *box);
extern int rbox_read_header(struct rbox_mailbox *mbox, struct sdbox_index_header *hdr, bool log_error,
                            bool *need_resize_r);

extern int rbox_mailbox_create_indexes(struct mailbox *box, const struct mailbox_update *update,
                                       struct mail_index_transaction *trans);
extern int check_users_mailbox_delete_ns_object(struct mail_user *user, librmb::RadosDovecotCephCfg *config,
                                                librmb::RadosNamespaceManager *ns_mgr, librmb::RadosStorage *storage);
/*
struct mail_save_context *rbox_save_alloc(struct mailbox_transaction_context *_t);
int rbox_save_begin(struct mail_save_context *ctx, struct istream *input);
int rbox_save_continue(struct mail_save_context *ctx);
int rbox_save_finish(struct mail_save_context *ctx);
void rbox_save_cancel(struct mail_save_context *ctx);

int rbox_transaction_save_commit_pre(struct mail_save_context *ctx);
void rbox_transaction_save_commit_post(struct mail_save_context *ctx,
                                       struct mail_index_transaction_commit_result *result);
void rbox_transaction_save_rollback(struct mail_save_context *ctx);
*/

#endif  // SRC_STORAGE_RBOX_RBOX_STORAGE_HPP_
