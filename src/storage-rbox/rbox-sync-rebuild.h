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

#ifndef SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_H_
#define SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_H_

#include <map>
#include <string>
#include <rados/librados.hpp>

#include "../librmb/rados-mail.h"

extern "C" {
#include "index-rebuild.h"
}

struct rbox_sync_rebuild_ctx {
  bool alt_storage;
  uint32_t next_uid;
};
extern void rbox_sync_update_header(struct index_rebuild_context *ctx);

extern int rbox_sync_add_object(struct index_rebuild_context *ctx, const std::string &oi, librmb::RadosMail *mail_obj,
                                bool alt_storage, uint32_t next_uid);

extern int rbox_sync_index_rebuild(struct index_rebuild_context *ctx, librados::NObjectIterator &iter,
                                   struct rbox_sync_rebuild_ctx *rebuild_ctx);
extern void rbox_sync_set_uidvalidity(struct index_rebuild_context *ctx);

extern int rbox_sync_index_rebuild_objects(struct index_rebuild_context *ctx);
extern int rbox_sync_rebuild_entry(struct index_rebuild_context *ctx, librados::NObjectIterator &iter,
                                   struct rbox_sync_rebuild_ctx *rebuild_ctx);
extern int rbox_sync_index_rebuild(struct rbox_mailbox *rbox, bool force);
extern int search_objects(struct index_rebuild_context *ctx, struct rbox_sync_rebuild_ctx *rebuild_ctx);
extern int rbox_storage_rebuild_in_context(struct rbox_storage *r_storage, bool force);
extern int repair_namespace(struct mail_namespace *ns, bool force, struct rbox_storage *r_storage);

#endif  // SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_H_
