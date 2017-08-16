/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_H_
#define SRC_STORAGE_RBOX_RBOX_SYNC_REBUILD_

#include <map>
#include <string>
#include <rados/librados.hpp>

extern "C" {
#include "index-rebuild.h"
}

extern int rbox_sync_add_object(struct index_rebuild_context *ctx, const std::string &oi,
                                std::map<std::string, ceph::bufferlist> &attrset);
extern int rbox_sync_index_rebuild(struct index_rebuild_context *ctx, std::string &mailbox_guid);
extern void rbox_sync_set_uidvalidity(struct index_rebuild_context *ctx);

extern int rbox_sync_index_rebuild_objects(struct index_rebuild_context *ctx);

extern int rbox_sync_index_rebuild(struct rbox_mailbox *mbox, bool force);

#endif
