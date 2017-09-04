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

#ifndef SRC_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_
#define SRC_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_

#ifdef HAVE_CONFIG_H
#include "dovecot-ceph-plugins-config.h"
#endif

struct module;

void dict_rados_plugin_init(struct module *module);
void dict_rados_plugin_deinit(void);

#endif  // SRC_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_
