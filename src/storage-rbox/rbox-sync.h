#ifndef RBOX_SYNC_H
#define RBOX_SYNC_H

struct mailbox;
struct rbox_mailbox;

enum rbox_sync_flags {
	RBOX_SYNC_FLAG_FORCE		= 0x01,
	RBOX_SYNC_FLAG_FSYNC		= 0x02,
	RBOX_SYNC_FLAG_FORCE_REBUILD	= 0x04
};

enum rbox_sync_entry_type {
	RBOX_SYNC_ENTRY_TYPE_EXPUNGE,
	RBOX_SYNC_ENTRY_TYPE_MOVE_FROM_ALT,
	RBOX_SYNC_ENTRY_TYPE_MOVE_TO_ALT
};

struct rbox_sync_context {
	struct rbox_mailbox *mbox;
        struct mail_index_sync_ctx *index_sync_ctx;
	struct mail_index_view *sync_view;
	struct mail_index_transaction *trans;
	enum rbox_sync_flags flags;
	ARRAY_TYPE(uint32_t) expunged_uids;
};

int rbox_sync_begin(struct rbox_mailbox *mbox, enum rbox_sync_flags flags,
		     struct rbox_sync_context **ctx_r);
int rbox_sync_finish(struct rbox_sync_context **ctx, bool success);
int rbox_sync(struct rbox_mailbox *mbox, enum rbox_sync_flags flags);

int rbox_sync_index_rebuild(struct rbox_mailbox *mbox, bool force);

struct mailbox_sync_context *
rbox_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags);

#endif
