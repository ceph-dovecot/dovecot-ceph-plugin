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
#include "mail-search-build.h"
#include "libdict-rados-plugin.h"
#include "mail-search-parser-private.h"
#include "mail-search.h"
}
#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"
#include "dovecot-ceph-plugin-config.h"
#include "../test-utils/it_utils.h"

using ::testing::AtLeast;
using ::testing::Return;

TEST_F(BackupTest, init) {}

TEST_F(BackupTest, mailbox_open_inbox) {
  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", MAILBOX_FLAG_READONLY);
  ASSERT_GE(mailbox_open(box), 0);
  mailbox_free(&box);
}

TEST_F(BackupTest, mail_copy_mail_in_inbox) {
  struct mailbox_transaction_context *desttrans;
  struct mail *mail;
  struct mail_search_context *search_ctx;
  struct mail_search_args *search_args;
  struct mail_search_arg *sarg;

  const char *message =
      "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  const char *mailbox = "INBOX";

  // create some testmails and delete the one with uid = 1

  testutils::ItUtils::add_mail(message, mailbox, BackupTest::s_test_mail_user->namespaces);
  testutils::ItUtils::add_mail(message, mailbox, BackupTest::s_test_mail_user->namespaces);
  testutils::ItUtils::add_mail(message, mailbox, BackupTest::s_test_mail_user->namespaces);
  testutils::ItUtils::add_mail(message, mailbox, BackupTest::s_test_mail_user->namespaces);

  // sleep(10);
  // rados_list_ctx_t listh;
  std::cout << "POOL: " << BackupTest::pool_name << std::endl;
  rados_ioctx_set_namespace(BackupTest::s_ioctx, "user-rbox-test@localhost_u");
  // rados_nobjects_list_open(BackupTest::s_ioctx, &listh);

  rados_list_ctx_t ctx;
  std::string to_delete;
  ASSERT_EQ(0, rados_nobjects_list_open(BackupTest::s_ioctx, &ctx));
  const char *entry;
  int foundit = 0;
  while (rados_nobjects_list_next(ctx, &entry, NULL, NULL) != -ENOENT) {
    foundit++;
    // ASSERT_EQ(std::string(entry), "foo");
    char xattr_res[100];

    ASSERT_EQ(rados_getxattr(BackupTest::s_ioctx, entry, "U", xattr_res, 1), 1);
    std::string v(&xattr_res[0], 1);
    if (v.compare("1") == 0) {
      to_delete = entry;
    }
    std::cout << std::string(entry) << std::endl;
  }
  ASSERT_EQ(foundit, 4);
  ASSERT_TRUE(!to_delete.empty());
  rados_nobjects_list_close(ctx);

  // delete UID=1
  ASSERT_EQ(rados_remove(BackupTest::s_ioctx, to_delete.c_str()), 0);

  // trigger the backup command!

  search_args = mail_search_build_init();
  sarg = mail_search_build_add(search_args, SEARCH_ALL);
  ASSERT_NE(sarg, nullptr);

  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);

  struct mailbox *box = mailbox_alloc(ns->list, mailbox, MAILBOX_FLAG_SAVEONLY);

  if (mailbox_open(box) < 0) {
    i_error("Opening mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
    FAIL() << " Forcing a resync on mailbox INBOX Failed";
  }

#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  desttrans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  memset(reason, '\0', sizeof(reason));
  desttrans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif

  search_ctx = mailbox_search_init(desttrans, search_args, NULL, static_cast<mail_fetch_field>(0), NULL);
  mail_search_args_unref(&search_args);
  struct message_size hdr_size, body_size;
  struct istream *input = NULL;
  while (mailbox_search_next(search_ctx, &mail)) {
    if (mail->uid == 1) {
      int ret = mail_get_stream(mail, &hdr_size, &body_size, &input);
      EXPECT_EQ(ret, -1);
      enum mail_error error;
      const char *errstr;
      errstr = mailbox_get_last_error(mail->box, &error);
      EXPECT_EQ(error, MAIL_ERROR_EXPUNGED);
    }
  }

  if (mailbox_search_deinit(&search_ctx) < 0) {
    FAIL() << "search deinit failed";
  }
  i_debug("after search");
  if (mailbox_transaction_commit(&desttrans) < 0) {
    FAIL() << "tnx commit failed";
  }
  i_debug("after commit");
  if (mailbox_sync(box, static_cast<mailbox_sync_flags>(0)) < 0) {
    FAIL() << "sync failed";
  }
  i_debug("after sync");
  mailbox_free(&box);
}

TEST_F(BackupTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
