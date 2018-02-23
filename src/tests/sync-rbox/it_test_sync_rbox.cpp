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

#include "libdict-rados-plugin.h"
}
#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"
#include "../test-utils/it_utils.h"

using ::testing::AtLeast;
using ::testing::Return;

TEST_F(SyncTest, init) {}

TEST_F(SyncTest, force_resync_missing_rados_object) {
  const char *message =
      "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  const char *mailbox = "INBOX";

  testutils::ItUtils::add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);
  testutils::ItUtils::add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);
  testutils::ItUtils::add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);

  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, mailbox, MAILBOX_FLAG_IGNORE_ACLS);

  if (mailbox_open(box) < 0) {
    i_error("Opening mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
    FAIL() << " Forcing a resync on mailbox INBOX Failed";
  } else {
    // removing one mail from rados!!
    struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
    librados::NObjectIterator iter(r_storage->s->get_io_ctx().nobjects_begin());
    std::string oid_to_delete;
    while (iter != librados::NObjectIterator::__EndObjectIterator) {
      oid_to_delete = (*iter).get_oid();
      i_debug("oid to delete: %s", oid_to_delete.c_str());
      iter++;
    }
    r_storage->s->delete_mail(oid_to_delete);

    // check index still has 3 entries
    uint32_t msg_count_before = mail_index_view_get_messages_count(box->view);
    i_debug("Message count before = %u", msg_count_before);
    EXPECT_EQ((uint32_t)3, msg_count_before);

    i_debug("ok starting rsync.");

    if (mailbox_sync(box, static_cast<mailbox_sync_flags>(MAILBOX_SYNC_FLAG_FORCE_RESYNC |
                                                          MAILBOX_SYNC_FLAG_FIX_INCONSISTENT)) < 0) {
      i_error("Forcing a resync on mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
      FAIL() << " Forcing a resync on mailbox INBOX Failed";
    }
    uint32_t msg_count = mail_index_view_get_messages_count(box->view);
    i_debug("Message count now = %u", msg_count);
    EXPECT_EQ((uint32_t)2, msg_count);
    // check index only has 2 entries
    uint32_t msg_count_new = msg_count_before - (uint32_t)1;
    EXPECT_EQ(msg_count, msg_count_new);
  }

  mailbox_free(&box);
}
/*
TEST_F(SyncTest, rsync_update_flags) {
  const char *message =
      "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  const char *mailbox = "INBOX";

  testutils::ItUtils::add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);

  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, mailbox, MAILBOX_FLAG_IGNORE_ACLS);

  if (mailbox_open(box) < 0) {
    i_error("Opening mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
    FAIL() << " Forcing a resync on mailbox INBOX Failed";
  } else {
    if (mailbox_sync(box, static_cast<mailbox_sync_flags>(MAILBOX_SYNC_FLAG_FAST)) < 0) {
      i_error("Forcing a resync on mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
      FAIL() << " Forcing a resync on mailbox INBOX Failed";
    }
    uint32_t msg_count = mail_index_view_get_messages_count(box->view);
    i_debug("Message count now = %u", msg_count);
    EXPECT_EQ((uint32_t)2, msg_count);
    // check index only has 2 entries
    uint32_t msg_count_new = msg_count_before - (uint32_t)1;
    EXPECT_EQ(msg_count, msg_count_new);
  }

  mailbox_free(&box);
}*/

TEST_F(SyncTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
