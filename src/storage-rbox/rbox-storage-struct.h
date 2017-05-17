/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_
#define SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_

#include "dbox-storage.h"
#include "rados-cluster.h"

class RadosStorage;

struct rbox_storage {
  struct dbox_storage storage;
  RadosCluster cluster;
  RadosStorage *s;
};

#endif /* SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_ */
