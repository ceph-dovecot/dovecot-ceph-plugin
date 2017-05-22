/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RADOS_RADOS_SAVE_H_
#define SRC_STORAGE_RADOS_RADOS_SAVE_H_

struct rados_save_context {
  struct mail_save_context ctx;

  struct rados_mailbox *mbox;
  struct mail_index_transaction *trans;

  char *tmp_basename;
  unsigned int mail_count;
  guid_128_t mail_guid;

  struct rados_sync_context *sync_ctx;

  /* updated for each appended mail: */
  uint32_t seq;
  struct istream *input;
  int fd;

  unsigned int failed : 1;
  unsigned int finished : 1;
};


#endif /* SRC_STORAGE_RADOS_RADOS_SAVE_H_ */
