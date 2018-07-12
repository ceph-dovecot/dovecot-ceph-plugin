// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "TestCase.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"           // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wundef"            // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wredundant-decls"  // turn off warnings for Dovecot :-(
#ifndef __cplusplus
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"  // turn off warnings for Dovecot :-(
#endif

extern "C" {

#include "lib.h"
#include "mail-user.h"
#include "mail-storage.h"
#include "mail-storage-service.h"
#include "mail-namespace.h"
#include "mailbox-list.h"

#include "ioloop.h"
#include "istream.h"
#include "mail-index.h"

#include "module-dir.h"
#include "str.h"
#include "hash.h"
#include "dict.h"
#include "imap-match.h"
#include "doveadm-mail.h"
#include "doveadm-rbox-plugin.h"
#include "libdict-rados-plugin.h"

}




#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"
#include "../test-utils/it_utils.h"

using ::testing::AtLeast;
using ::testing::Return;

void doveadm_register_cmd(const struct doveadm_cmd *cmd) {

}
const char *doveadm_plugin_getenv(const char *name) {
	return NULL;
}

/* some bullshit wrapper */
void doveadm_mail_help_name(const char *cmd_name) {
  // doveadm_mail_try_help_name(cmd_name);
  // i_fatal("Missing help for command %s", cmd_name);
}

void doveadm_mailbox_args_check(const char *const args[]) {}
struct doveadm_mail_cmd_context *doveadm_mail_cmd_alloc_size(size_t size) {
  struct doveadm_mail_cmd_context *ctx;
  pool_t pool;

  i_assert(size >= sizeof(struct doveadm_mail_cmd_context));

  pool = pool_alloconly_create("doveadm mail cmd", 1024);
  ctx = p_malloc(pool, size);
  ctx->pool = pool;
  return ctx;
}

int doveadm_mail_server_user(struct doveadm_mail_cmd_context *ctx, const struct mail_storage_service_input *input,
                             const char **error_r) {
  return 0;
}
void doveadm_mail_register_cmd(const struct doveadm_mail_cmd *cmd) {}
void doveadm_mail_failed_mailbox(struct doveadm_mail_cmd_context *ctx, struct mailbox *box) {}
void doveadm_mail_failed_error(struct doveadm_mail_cmd_context *ctx, enum mail_error error) {}

TEST_F(SyncTest, init) {
}

TEST_F(SyncTest, test_doveadm) {
	cmd_rmb_lspools(0, NULL);

}


TEST_F(SyncTest, deinit) {
}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
