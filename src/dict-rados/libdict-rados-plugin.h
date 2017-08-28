/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_
#define SRC_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_

#ifdef HAVE_CONFIG_H
#include "dovecot-ceph-plugins-config.h"
#endif

struct module;

void dict_rados_plugin_init(struct module *module);
void dict_rados_plugin_deinit(void);

#endif /* SRC_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_ */
