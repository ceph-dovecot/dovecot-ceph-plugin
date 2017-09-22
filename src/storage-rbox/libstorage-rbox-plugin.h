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

#ifndef SRC_STORAGE_RBOX_LIBSTORAGE_RBOX_PLUGIN_H_
#define SRC_STORAGE_RBOX_LIBSTORAGE_RBOX_PLUGIN_H_

#ifdef HAVE_CONFIG_H
#include "dovecot-ceph-plugin-config.h"
#endif

void storage_rbox_plugin_init(struct module *module);
void storage_rbox_plugin_deinit(void);

#endif  // SRC_STORAGE_RBOX_LIBSTORAGE_RBOX_PLUGIN_H_
