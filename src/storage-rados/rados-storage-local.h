/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RADOS_RADOS_STORAGE_LOCAL_H_
#define SRC_STORAGE_RADOS_RADOS_STORAGE_LOCAL_H_

#include "index-storage.h"

#define RADOS_STORAGE_NAME "rados"

#define RADOS_SUBSCRIPTION_FILE_NAME "subscriptions"
#define RADOS_UIDVALIDITY_FILE_NAME "dovecot-uidvalidity"
#define RADOS_TEMP_FILE_PREFIX ".temp."

#define RADOS_MAILBOX_DIR_NAME "mailboxes"
#define RADOS_TRASH_DIR_NAME "trash"
#define RADOS_MAILDIR_NAME "rados-Mails"

#define SDBOX_INDEX_HEADER_MIN_SIZE (sizeof(uint32_t))
struct sdbox_index_header {
  /* increased every time a full mailbox rebuild is done */
  uint32_t rebuild_count;
  guid_128_t mailbox_guid;
  uint8_t flags; /* enum dbox_index_header_flags */
  uint8_t unused[3];
};

struct rados_storage__ {
  struct mail_storage storage;
  //  RadosCluster cluster;
  //  RadosStorage *s;
};

struct obox_mail_index_record {
  unsigned char guid[GUID_128_SIZE];
  unsigned char oid[GUID_128_SIZE];
};

struct rados_mailbox {
  struct mailbox box;
  struct rados_storage *storage;

  uint32_t hdr_ext_id;
  uint32_t ext_id;

  guid_128_t mailbox_guid;
};

/* Globally unique identifier for the message. Preserved when
   copying. */
#define RBOX_METADATA_ALL "METADATA"
#define RBOX_METADATA_GUID "G"
/* POP3 UIDL overriding the default format */
#define RBOX_METADATA_POP3_UIDL "P"
/* POP3 message ordering (for migrated mails) */
#define RBOX_METADATA_POP3_ORDER "O"
/* Received UNIX timestamp in hex */
#define RBOX_METADATA_RECEIVED_DATE "R"
/* Physical message size in hex. Necessary only if it differs from
   the dbox_message_header.message_size_hex, for example because the
   message is compressed. */
#define RBOX_METADATA_PHYSICAL_SIZE "Z"
/* Virtual message size in hex (line feeds counted as CRLF) */
#define RBOX_METADATA_VIRTUAL_SIZE "V"
/* Pointer to external message data. Format is:
   1*(<start offset> <byte count> <options> <ref>) */
#define RBOX_METADATA_EXT_REF "X"
/* Mailbox name where this message was originally saved to.
   When rebuild finds a message whose mailbox is unknown, it's
   placed to this mailbox. */
#define RBOX_METADATA_ORIG_MAILBOX "B"

extern struct mail_vfuncs rados_mail_vfuncs;

extern struct mail_save_context *rados_save_alloc(struct mailbox_transaction_context *_t);
extern int rados_save_begin(struct mail_save_context *ctx, struct istream *input);
extern int rados_save_continue(struct mail_save_context *ctx);
extern int rados_save_finish(struct mail_save_context *ctx);
extern void rados_save_cancel(struct mail_save_context *ctx);

extern int rados_transaction_save_commit_pre(struct mail_save_context *ctx);
extern void rados_transaction_save_commit_post(struct mail_save_context *ctx,
                                               struct mail_index_transaction_commit_result *result);
extern void rados_transaction_save_rollback(struct mail_save_context *ctx);

extern int rados_mailbox_open(struct mailbox *box);
extern int rados_mailbox_create(struct mailbox *box, const struct mailbox_update *update, bool directory);
extern int rados_mailbox_get_metadata(struct mailbox *box, enum mailbox_metadata_items items,
                                      struct mailbox_metadata *metadata_r);

extern struct mailbox_sync_context *rados_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags);
extern void rados_notify_changes(struct mailbox *box);

extern struct mail_storage *rados_storage_alloc(void);
extern void rados_storage_get_list_settings(const struct mail_namespace *ns ATTR_UNUSED,
                                            struct mailbox_list_settings *set);

extern struct mailbox *rados_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname,
                                           enum mailbox_flags flags);

struct mail_save_context *rados_save_alloc(struct mailbox_transaction_context *_t);
int rados_save_begin(struct mail_save_context *ctx, struct istream *input);
int rados_save_continue(struct mail_save_context *ctx);
int rados_save_finish(struct mail_save_context *ctx);
void rados_save_cancel(struct mail_save_context *ctx);

int rados_transaction_save_commit_pre(struct mail_save_context *ctx);
void rados_transaction_save_commit_post(struct mail_save_context *ctx,
                                        struct mail_index_transaction_commit_result *result);
void rados_transaction_save_rollback(struct mail_save_context *ctx);

#endif  // SRC_STORAGE_RADOS_RADOS_STORAGE_LOCAL_H_
