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
#ifndef SRC_DOVEADM_RBOX_PLUGIN_H
#define SRC_DOVEADM_RBOX_PLUGIN_H

extern struct doveadm_mail_cmd_context *cmd_rmb_ls_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_get_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_set_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_delete_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_ls_mb_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_rename_alloc(void);

extern struct doveadm_mail_cmd_context *cmd_rmb_revert_log_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_check_indices_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_mailbox_delete_alloc(void);

extern void cmd_rmb_config_show(int argc, char *argv[]);
extern void cmd_rmb_config_create(int argc, char *argv[]);
extern void cmd_rmb_config_update(int argc, char *argv[]);
extern void cmd_rmb_lspools(int argc, char *argv[]);
extern void cmd_rmb_rename(int argc, char *argv[]);

extern struct doveadm_mail_cmd_context *cmd_rmb_save_log_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_check_indices_alloc(void);
extern struct doveadm_mail_cmd_context *cmd_rmb_mailbox_delete_alloc(void);

#endif  // SRC_DOVEADM_RBOX_PLUGIN_H_
