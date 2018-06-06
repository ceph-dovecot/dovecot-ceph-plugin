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
extern "C" {
#include "lib.h"
#include "module-dir.h"
#include "str.h"
#include "hash.h"
#include "dict.h"
#include "imap-match.h"
#include "doveadm-settings.h"
#include "doveadm-mail.h"
#include "doveadm-rbox-plugin.h"
}
#include "tools/rmb/rmb-commands.h"

static int cmd_rmb_optimize_run(struct doveadm_mail_cmd_context *ctx, struct mail_user *user) {
  //  const char *ns_prefix = ctx->args[0];
  librmb::RmbCommands::RmbCommands::lspools();
  return 0;
}

static void cmd_rmb_optimize_init(struct doveadm_mail_cmd_context *ctx ATTR_UNUSED, const char *const args[]) {
  if (str_array_length(args) > 0) {
    doveadm_mail_help_name("rmb lspools");
  }
}

struct doveadm_mail_cmd_context *cmd_rmb_lspools_alloc(void) {
  struct doveadm_mail_cmd_context *ctx;
  ctx = doveadm_mail_cmd_alloc(struct doveadm_mail_cmd_context);
  ctx->v.run = cmd_rmb_optimize_run;
  ctx->v.init = cmd_rmb_optimize_init;
  return ctx;
}
