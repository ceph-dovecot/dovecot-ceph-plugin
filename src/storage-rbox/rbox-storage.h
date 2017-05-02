#ifndef RBOX_STORAGE_H
#define RBOX_STORAGE_H

#include "index-storage.h"
#include "dbox-storage.h"

#define RBOX_STORAGE_NAME "rbox"
#define RBOX_MAIL_FILE_PREFIX "u."
#define RBOX_MAIL_FILE_FORMAT RBOX_MAIL_FILE_PREFIX"%u"

#define RBOX_INDEX_HEADER_MIN_SIZE (sizeof(uint32_t))
struct rbox_index_header {
	/* increased every time a full mailbox rebuild is done */
	uint32_t rebuild_count;
	guid_128_t mailbox_guid;
	uint8_t flags; /* enum dbox_index_header_flags */
	uint8_t unused[3];
};

struct rbox_storage {
	struct dbox_storage storage;
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

	/* if non-zero, storage should be rebuilt (except if rebuild_count
	 has changed from this value) */
	uint32_t corrupted_rebuild_count;

	guid_128_t mailbox_guid;
};

extern struct mail_vfuncs rbox_mail_vfuncs;

struct mail *
rbox_mail_alloc(struct mailbox_transaction_context *t, enum mail_fetch_field wanted_fields,
		struct mailbox_header_lookup_ctx *wanted_headers);
int rbox_mail_open(struct dbox_mail *mail, uoff_t *offset_r, struct dbox_file **file_r);

int rbox_read_header(struct rbox_mailbox *mbox, struct rbox_index_header *hdr, bool log_error, bool *need_resize_r);
int rbox_mailbox_create_indexes(struct mailbox *box, const struct mailbox_update *update, struct mail_index_transaction *trans);
void rbox_set_mailbox_corrupted(struct mailbox *box);

struct mail_save_context *
rbox_save_alloc(struct mailbox_transaction_context *_t);
int rbox_save_begin(struct mail_save_context *ctx, struct istream *input);
int rbox_save_continue(struct mail_save_context *_ctx);
int rbox_save_finish(struct mail_save_context *ctx);
void rbox_save_cancel(struct mail_save_context *ctx);

struct dbox_file *
rbox_save_file_get_file(struct mailbox_transaction_context *t, uint32_t seq);
void rbox_save_add_file(struct mail_save_context *ctx, struct dbox_file *file);

int rbox_transaction_save_commit_pre(struct mail_save_context *ctx);
void rbox_transaction_save_commit_post(struct mail_save_context *ctx, struct mail_index_transaction_commit_result *result);
void rbox_transaction_save_rollback(struct mail_save_context *ctx);

int rbox_copy(struct mail_save_context *ctx, struct mail *mail);

#endif
