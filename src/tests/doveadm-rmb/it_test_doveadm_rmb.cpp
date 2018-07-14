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
#include "master-service.h"
#include "master-service-settings.h"
#include "doveadm-settings.h"
#include "mempool.h"
}

#include <sstream>  // std::stringstream

#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"
#include "../test-utils/it_utils.h"

using ::testing::AtLeast;
using ::testing::Return;

void doveadm_register_cmd(const struct doveadm_cmd *cmd) {
}
const char *doveadm_plugin_getenv(const char *name) {
  if (strcmp(name, "rbox_pool_name") == 0) {
    return DoveadmTest::get_pool_name();
  }
  return NULL;
}

void doveadm_mail_help_name(const char *cmd_name) {
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


TEST_F(DoveadmTest, init) {
/*  struct master_service *master_service;
  enum master_service_flags service_flags = MASTER_SERVICE_FLAG_KEEP_CONFIG_OPEN;
  const char *error;

  int argc = 2;
  char **argv = malloc(sizeof(char *) * 2);
  argv[0] = static_cast<char *>(malloc(sizeof(char) * 10));
  argv[1] = static_cast<char *>(malloc(sizeof(char) * 10));

  master_service = master_service_init("doveadm", service_flags, &argc, &argv, "");

  std::cout << " hallo service" << master_service << std::endl;
  if (master_getopt(master_service) > 0) {
    std::cout << " no opts" << std::endl;
  }


  master_service_init_finish(master_service);
  master_service_deinit(&master_service);*/
}

TEST_F(DoveadmTest, test_doveadm) { ASSERT_EQ(cmd_rmb_lspools(0, NULL), 0); }
TEST_F(DoveadmTest, test_create_config) { ASSERT_EQ(cmd_rmb_config_create(0, NULL), 0); }
TEST_F(DoveadmTest, test_show_config) { ASSERT_EQ(cmd_rmb_config_show(0, NULL), 0); }

TEST_F(DoveadmTest, test_update_config_invalid_key) {
  char *argv[] = {"rmb", "invalid_key=1"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, -1);
}

TEST_F(DoveadmTest, test_update_config_valid_key) {
  char *argv[] = {"rmb", "user_mapping=false"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, 0);
}

TEST_F(DoveadmTest, cmd_rmb_ls_empty_box) {
  char *argv[] = {"ls", "-"};

  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_ls_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t1";
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_ls_mail_invalid_mail) {
  char *argv[] = {"ls", "-"};

  rados_ioctx_set_namespace(DoveadmTest::get_io_ctx(), "t1_u");
  ASSERT_EQ(rados_write(DoveadmTest::get_io_ctx(), "hw", "Hello World!", 12, 0), 0);

  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_ls_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t1";
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_get_mail_valid_mail) {
  char *argv[] = {"-", "test_get"};
  rados_ioctx_set_namespace(DoveadmTest::get_io_ctx(), "t1_u");
  ASSERT_EQ(rados_write(DoveadmTest::get_io_ctx(), "hw", "Hello World!", 12, 0), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "B", "INBOX", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "G", "ksksk", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "I", "0.1", 3), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "M", "MY_BOX", 6), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "R", "1531485201", 10), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "V", "2256", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "Z", "2210", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw", "U", "1", 1), 0);

  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_get_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t1";
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);
  struct stat buf;

  ASSERT_EQ(stat("test_get/MY_BOX/1.hw", &buf), 0);
  remove("test_get/MY_BOX/1.hw");
  remove("test_get/MY_BOX");
  remove("test_get");
}

TEST_F(DoveadmTest, cmd_rmb_get_param_check) {
  const char *const argv[] = {"-"};
  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_get_alloc();
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.init(cmd_ctx, argv);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_set_mail_attr) {
  char *argv[] = {"hw2", "B=INBOX2"};
  rados_ioctx_set_namespace(DoveadmTest::get_io_ctx(), "t1_u");
  ASSERT_EQ(rados_write(DoveadmTest::get_io_ctx(), "hw2", "Hello World!", 12, 0), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "B", "INBOX", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "G", "ksksk", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "I", "0.1", 3), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "M", "MY_BOX", 6), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "R", "1531485201", 10), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "V", "2256", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "Z", "2210", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "U", "1", 1), 0);

  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_set_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t1";
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);

  char xattr_res[100];
  ASSERT_EQ(rados_getxattr(DoveadmTest::get_io_ctx(), "hw2", "B", xattr_res, 6), 6);

  std::string v(&xattr_res[0], 6);
  ASSERT_EQ(v.compare("INBOX2"), 0);
}

TEST_F(DoveadmTest, cmd_rmb_set_mail_invalid_attr) {
  char *argv[] = {"hw2", "B2=INBOX2"};  // update invalid attribute
  rados_ioctx_set_namespace(DoveadmTest::get_io_ctx(), "t1_u");
  ASSERT_EQ(rados_write(DoveadmTest::get_io_ctx(), "hw2", "Hello World!", 12, 0), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "B", "INBOX", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "G", "ksksk", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "I", "0.1", 3), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "M", "MY_BOX", 6), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "R", "1531485201", 10), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "V", "2256", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "Z", "2210", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "U", "1", 1), 0);

  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_set_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t1";
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, -1);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_rename_user) {
  char *argv[] = {"rmb", "user_mapping=true"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, 0);

  rados_ioctx_set_namespace(DoveadmTest::get_io_ctx(), "users");
  ASSERT_EQ(rados_write(DoveadmTest::get_io_ctx(), "t1_u", "ABCDEFG", 12, 0), 0);

  char *argv2[] = {"t_22"};
  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_rename_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t1";
  cmd_ctx->args = argv2;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);

  uint64_t size;
  time_t tm;

  ASSERT_EQ(rados_stat(DoveadmTest::get_io_ctx(), "t_22_u", &size, &tm), 0);
  ASSERT_EQ(rados_stat(DoveadmTest::get_io_ctx(), "t1_u", &size, &tm), -2);

  char read_res[100];
  ASSERT_EQ(rados_read(DoveadmTest::get_io_ctx(), "t_22_u", read_res, 7, 0), 7);
  read_res[8] = '\0';
  std::string v(&read_res[0], 7);
  ASSERT_EQ(v.compare("ABCDEFG\0"), 0);
}

TEST_F(DoveadmTest, cmd_rmb_rename_user_no_indirect_user_mapping) {
  char *argv[] = {"rmb", "user_mapping=false"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, 0);

  char *argv2[] = {"t_22"};
  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_rename_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t1";
  cmd_ctx->args = argv2;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, -1);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_rename_unknown_user) {
  char *argv[] = {"rmb", "user_mapping=true"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, 0);

  char *argv2[] = {"t_22"};
  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_rename_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "t2222";  // unknown user!
  cmd_ctx->args = argv2;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, -1);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_check_indices) {
  char *argv[] = {"rmb", "user_mapping=false"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, 0);

  std::stringstream ss;

  ss << DoveadmTest::s_test_mail_user->username << "_u";
  std::string ns = ss.str();
  // add new object
  rados_ioctx_set_namespace(DoveadmTest::get_io_ctx(), ns.c_str());
  ASSERT_EQ(rados_write(DoveadmTest::get_io_ctx(), "hw2", "Hello World!", 12, 0), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "B", "INBOX", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "G", "ksksk", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "I", "0.1", 3), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "M", "MY_BOX", 6), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "R", "1531485201", 10), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "V", "2256", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "Z", "2210", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "U", "1", 1), 0);

  char *argv2[] = {DoveadmTest::s_test_mail_user->username};
  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_check_indices_alloc();

  cmd_ctx->args = argv2;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, DoveadmTest::s_test_mail_user);
  ASSERT_EQ(cmd_ctx->exit_code, 1);
  pool_unref(&cmd_ctx->pool);

}

TEST_F(DoveadmTest, cmd_rmb_check_indices_delete) {
  char *argv[] = {"rmb", "user_mapping=false"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, 0);

  std::stringstream ss;

  ss << DoveadmTest::s_test_mail_user->username << "_u";
  std::string ns = ss.str();
  // add new object
  rados_ioctx_set_namespace(DoveadmTest::get_io_ctx(), ns.c_str());
  ASSERT_EQ(rados_write(DoveadmTest::get_io_ctx(), "hw2", "Hello World!", 12, 0), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "B", "INBOX", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "G", "ksksk", 5), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "I", "0.1", 3), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "M", "MY_BOX", 6), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "R", "1531485201", 10), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "V", "2256", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "Z", "2210", 4), 0);
  ASSERT_EQ(rados_setxattr(DoveadmTest::get_io_ctx(), "hw2", "U", "1", 1), 0);

  char *argv2[] = {DoveadmTest::s_test_mail_user->username};
  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_check_indices_alloc();
  struct check_indices_cmd_context *ctx_ = (struct check_indices_cmd_context *)cmd_ctx;
  ctx_->delete_not_referenced_objects = true;
  cmd_ctx->args = argv2;
  cmd_ctx->iterate_single_user = true;
  cmd_ctx->v.run(cmd_ctx, DoveadmTest::s_test_mail_user);
  ASSERT_EQ(cmd_ctx->exit_code, 2);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_delete_mailbox) {
  char *argv[] = {"rmb", "user_mapping=false"};
  int ret = cmd_rmb_config_update(2, argv);
  ASSERT_EQ(ret, 0);

  std::stringstream ss;

  char *argv2[] = {"INBOX"};
  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_mailbox_delete_alloc();
  struct delete_cmd_context *ctx = (struct delete_cmd_context *)cmd_ctx;
  ctx->recursive = TRUE;
  cmd_ctx->args = argv2;
  cmd_ctx->iterate_single_user = true;
  char *name = p_strdup(cmd_ctx->pool, argv2[0]);
  array_append(&ctx->mailboxes, &name, 1);

  cmd_ctx->v.run(cmd_ctx, DoveadmTest::s_test_mail_user);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);
}

TEST_F(DoveadmTest, cmd_rmb_delete) {
  char *argv[] = {"rbox_cfg"};

  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_delete_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "";
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  int ret = cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, 0);
  pool_unref(&cmd_ctx->pool);
}
TEST_F(DoveadmTest, cmd_rmb_delete_no_object) {
  char *argv[] = {"no_obj"};

  struct doveadm_mail_cmd_context *cmd_ctx = cmd_rmb_delete_alloc();
  struct mail_user *user = p_new(cmd_ctx->pool, struct mail_user, 1);
  user->username = "";
  cmd_ctx->args = argv;
  cmd_ctx->iterate_single_user = true;
  int ret = cmd_ctx->v.run(cmd_ctx, user);
  ASSERT_EQ(cmd_ctx->exit_code, -2);
  pool_unref(&cmd_ctx->pool);
}
TEST_F(DoveadmTest, deinit) {

}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
