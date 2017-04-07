#ifndef LIBDICT_RADOS_PLUGIN_H
#define LIBDICT_RADOS_PLUGIN_H

struct module;

void dict_rados_plugin_init(struct module *module);
void dict_rados_plugin_deinit(void);

#endif
