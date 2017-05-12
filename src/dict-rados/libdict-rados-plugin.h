/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef DOVECOT_RADOS_PLUGIN_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_
#define DOVECOT_RADOS_PLUGIN_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_

struct module;

void dict_rados_plugin_init(struct module *module);
void dict_rados_plugin_deinit(void);

#endif /* DOVECOT_RADOS_PLUGIN_DICT_RADOS_LIBDICT_RADOS_PLUGIN_H_ */
