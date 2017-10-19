// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_STORAGE_RBOX_RBOX_SAVE_H_
#define SRC_STORAGE_RBOX_RBOX_SAVE_H_

#include <string>
#include <vector>

#include "../librmb/rados-storage-impl.h"
#include "mail-storage-private.h"

#include "rados-mail-object.h"

class rbox_save_context {
 public:
  explicit rbox_save_context(const librmb::RadosStorage &_rados_storage)
      : ctx({}),
        mbox(NULL),
        trans(NULL),
        mail_count(0),
        sync_ctx(NULL),
        seq(0),
        input(NULL),
        output_stream(NULL),
        rados_storage(_rados_storage),
        current_object(NULL),
        failed(1),
        finished(1),
        copying(0),
        dest_mail_allocated(0) {}

  struct mail_save_context ctx;

  struct rbox_mailbox *mbox;
  struct mail_index_transaction *trans;

  unsigned int mail_count;

  guid_128_t mail_guid;  // goes to index record
  guid_128_t mail_oid;   // goes to index record

  struct rbox_sync_context *sync_ctx;

  /* updated for each appended mail: */
  uint32_t seq;
  struct istream *input;
  struct ostream *output_stream;

  const librmb::RadosStorage &rados_storage;
  std::vector<librmb::RadosMailObject *> objects;
  librmb::RadosMailObject *current_object;

  unsigned int failed : 1;
  unsigned int finished : 1;
  unsigned int copying : 1;
  unsigned int dest_mail_allocated : 1;
};

void rbox_add_to_index(struct mail_save_context *_ctx);
void rbox_move_index(struct mail_save_context *_ctx, struct mail *src_mail);
void init_output_stream(mail_save_context *_ctx);
int allocate_mail_buffer(mail_save_context *_ctx, int &initial_mail_buffer_size);
void clean_up_mail_object_list(struct rbox_save_context *r_ctx, struct rbox_storage *r_storage);
#endif  // SRC_STORAGE_RBOX_RBOX_SAVE_H_
