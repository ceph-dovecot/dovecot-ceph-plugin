/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RADOS_RADOS_SYNC_H_
#define SRC_STORAGE_RADOS_RADOS_SYNC_H_

struct mailbox;

enum rados_sync_flags {
  RADOS_SYNC_FLAG_FORCE = 0x01,
  RADOS_SYNC_FLAG_FSYNC = 0x02,
  RADOSSYNC_FLAG_FORCE_REBUILD = 0x04
};

struct rados_sync_context {
  struct rados_mailbox *mbox;
  struct mail_index_sync_ctx *index_sync_ctx;
  struct mail_index_view *sync_view;
  struct mail_index_transaction *trans;

  string_t *path;
  size_t path_dir_prefix_len;
  uint32_t uid_validity;
};

int rados_sync(struct rados_mailbox *mbox);

int rados_sync_begin(struct rados_mailbox *mbox, struct rados_sync_context **ctx_r, bool force);
int rados_sync_finish(struct rados_sync_context **ctx, bool success);

struct mailbox_sync_context *rados_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags);

#endif /* SRC_STORAGE_RADOS_RADOS_SYNC_H_ */
