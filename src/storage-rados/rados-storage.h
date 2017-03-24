#ifndef RADOS_STORAGE_H
#define RADOS_STORAGE_H

#include "index-storage.h"

#define RADOS_STORAGE_NAME "rados"
#define RADOS_SUBSCRIPTION_FILE_NAME "subscriptions."

struct rados_storage {
	struct mail_storage storage;
};

struct rados_mailbox {
	struct mailbox box;
	struct rados_storage *storage;

	guid_128_t mailbox_guid;
};

extern struct mail_vfuncs rados_mail_vfuncs;

struct mail_save_context *
rados_save_alloc(struct mailbox_transaction_context *_t);
int rados_save_begin(struct mail_save_context *ctx, struct istream *input);
int rados_save_continue(struct mail_save_context *ctx);
int rados_save_finish(struct mail_save_context *ctx);
void rados_save_cancel(struct mail_save_context *ctx);

int rados_transaction_save_commit_pre(struct mail_save_context *ctx);
void rados_transaction_save_commit_post(struct mail_save_context *ctx,
					struct mail_index_transaction_commit_result *result);
void rados_transaction_save_rollback(struct mail_save_context *ctx);

#endif
