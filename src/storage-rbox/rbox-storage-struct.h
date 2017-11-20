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

#ifndef SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_
#define SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_

#include "../librmb/rados-cluster-impl.h"
#include "../librmb/rados-storage-impl.h"
#include "../librmb/rados-namespace-manager.h"

struct rbox_storage {
  struct mail_storage storage;

  librmb::RadosCluster* cluster;
  librmb::RadosStorage *s;
  librmb::RadosNamespaceManager *ns_mgr;
};

#endif  // SRC_STORAGE_RBOX_RBOX_STORAGE_STRUCT_H_
