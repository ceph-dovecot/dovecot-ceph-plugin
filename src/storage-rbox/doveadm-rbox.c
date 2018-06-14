// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 * Copyright (c) 2007-2017 Dovecot authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
#include "lib.h"
#include "module-dir.h"
#include "str.h"
#include "hash.h"
#include "dict.h"
#include "imap-match.h"
#include "doveadm-settings.h"
#include "doveadm-mail.h"

#include "doveadm-rbox-plugin.h"

#define DOVEADM_EXPIRE_MAIL_CMD_CONTEXT(obj) MODULE_CONTEXT(obj, doveadm_expire_mail_cmd_module)
const char *doveadm_rbox_plugin_version = DOVECOT_ABI_VERSION;

static struct doveadm_mail_cmd rmb_commands[] = {
    {cmd_rmb_lspools_alloc, "rmb lspools", NULL},
    {cmd_rmb_config_create_alloc, "rmb config create", NULL},
    {cmd_rmb_config_show_alloc, "rmb config show", NULL},
    {cmd_rmb_config_update_alloc, "rmb config update", "key=value"},
    {cmd_rmb_ls_alloc, "rmb ls", "-|key=value", "uid|recv_date|save_date|phy_size"},
    {cmd_rmb_get_alloc, "rmb get", "-|key=value", "output path", "uid|recv_date|save_date|phy_size"},
    {cmd_rmb_set_alloc, "rmb set", "oid", "key=value"},
    {cmd_rmb_delete_alloc, "rmb delete", "oid"},
    {cmd_rmb_ls_mb_alloc, "rmb ls mb", NULL},
    {cmd_rmb_rename_alloc, "rmb rename", "new username"},
    {cmd_rmb_save_log_alloc, "rmb save_log", "path to save_log"},
    {cmd_rmb_check_indices_alloc, "rmb check indices", "delete_not_referenced_objects"}};

void doveadm_rbox_plugin_init(struct module *module ATTR_UNUSED) {
  unsigned int i;
  for (i = 0; i < N_ELEMENTS(rmb_commands); i++) {
    doveadm_mail_register_cmd(&rmb_commands[i]);
  }
}

void doveadm_rbox_plugin_deinit(void) {}
