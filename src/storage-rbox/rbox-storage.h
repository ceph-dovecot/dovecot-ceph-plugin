/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_RBOX_STORAGE_H_
#define SRC_STORAGE_RBOX_RBOX_STORAGE_H_

#define RBOX_STORAGE_NAME "rbox"

#ifdef __cplusplus
extern "C" {
#endif
#include "index-storage.h"

extern int rbox_storage_create(struct mail_storage *storage, struct mail_namespace *ns, const char **error_r);
extern struct mail_storage *rbox_storage_alloc(void);
extern void rbox_storage_destroy(struct mail_storage *storage);
extern void rbox_storage_get_list_settings(const struct mail_namespace *ns, struct mailbox_list_settings *set);
extern struct mailbox *rbox_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname,
                                          enum mailbox_flags flags);

extern struct mail_storage rbox_storage;

#ifdef __cplusplus
}
#endif

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
};

#endif  // SRC_STORAGE_RBOX_RBOX_STORAGE_H_
