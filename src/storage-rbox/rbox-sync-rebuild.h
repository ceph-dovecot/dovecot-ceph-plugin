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

#ifndef SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_H_
#define SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_H_

#include <map>
#include <string>
#include <rados/librados.hpp>

extern "C" {
#include "index-rebuild.h"
}

extern int rbox_sync_add_object(struct index_rebuild_context *ctx, const std::string &oi,
                                const std::map<std::string, ceph::bufferlist> &attrset);
extern int rbox_sync_index_rebuild(struct index_rebuild_context *ctx, const std::string &mailbox_guid);
extern void rbox_sync_set_uidvalidity(struct index_rebuild_context *ctx);

extern int rbox_sync_index_rebuild_objects(struct index_rebuild_context *ctx);

extern int rbox_sync_index_rebuild(struct rbox_mailbox *mbox, bool force);

#endif  // SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_H_
