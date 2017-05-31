/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RADOS_RADOS_STORAGE_STRUCT_H_
#define SRC_STORAGE_RADOS_RADOS_STORAGE_STRUCT_H_

#include "mail-storage-private.h"
#include "rados-cluster.h"
#include "rados-storage.h"

struct rados_storage {
  struct mail_storage storage;
  librmb::RadosCluster cluster;
  librmb::RadosStorage *s;
};

#endif /* SRC_STORAGE_RADOS_RADOS_STORAGE_STRUCT_H_ */
