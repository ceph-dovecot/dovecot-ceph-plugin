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
#include "rados-util.h"
#include "rbox-storage.h"

using ::testing::AtLeast;
using ::testing::Return;

TEST_F(SyncTest, init) {}

static void copy_object(struct mail_namespace *_ns, struct mailbox *box) {
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  librmb::RadosMetadata xattr(librmb::rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, box->name);
  librados::NObjectIterator iter = r_storage->s->find_mails(&xattr);

  std::string oid;
  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    oid = iter->get_oid();
    iter++;
  }
  EXPECT_TRUE(oid.length() > 0);

  guid_128_t temp_oid_guid;
  guid_128_generate(temp_oid_guid);

  std::string test_oid = guid_128_to_string(temp_oid_guid);

  librados::ObjectWriteOperation write_op;
  // last version is always version of object with oid.
  // wait for test cluster (object exists...)
  uint64_t size = 0;
  time_t pmtime = 0;
  int stat_ret = r_storage->s->get_io_ctx().stat(oid, &size, &pmtime);
  EXPECT_NE(size, 0);
  i_debug("Last Version = %lu for obj: %s , stat =%d, %ld", r_storage->s->get_io_ctx().get_last_version(), oid.c_str(),
          stat_ret, size);

#if LIBRADOS_VERSION_CODE >= 30000
  write_op.copy_from(oid, r_storage->s->get_io_ctx(), r_storage->s->get_io_ctx().get_last_version(), 0);
#else
  write_op.copy_from(oid, r_storage->s->get_io_ctx(), r_storage->s->get_io_ctx().get_last_version());
#endif

  int ret = r_storage->s->get_io_ctx().operate(test_oid, &write_op);

  i_debug("copy operate: %d for %s", ret, test_oid.c_str());
  EXPECT_EQ(ret, 0);

  const char *metadata_name = "U";
  librados::bufferlist list;
  std::string id = "100";
  list.append(id.c_str(), id.length() + 1);
  ret = r_storage->s->get_io_ctx().setxattr(test_oid, metadata_name, list);
  i_debug("copy operate setxattr: %d for %s", ret, test_oid.c_str());

  if (rbox_open_rados_connection(box, true) < 0) {
    FAIL() << "error opening connection";
  }

  if (librmb::RadosUtils::move_to_alt(test_oid, r_storage->s, r_storage->alt, r_storage->ms, false) < 0) {
    FAIL() << "error moving object to alt storage";
  }
  i_debug("ok, mail moved to alt");

  EXPECT_EQ(ret, 0);
}
/**
 * - save mail via regular dovecot api calls
 * - copy mail with helper function and change xattr. uid
 * - move mail to alt_storage
 * - call mailbox_sync to repair box
 * - validate number of valid mails in index is 2.
 */
TEST_F(SyncTest, force_resync_restore_missing_index_entry) {
  const char *message =
      "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  const char *mailbox = "INBOX";

  // create only one mail.
  testutils::ItUtils::add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);

  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);

  struct mailbox *box = mailbox_alloc(ns->list, mailbox, MAILBOX_FLAG_IGNORE_ACLS);
  std::string alt_dir = "mail_storage_alt_test_sync";
  box->list->set.alt_dir = alt_dir.c_str();
  if (mailbox_open(box) < 0) {
    i_error("Opening mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
    FAIL() << " Forcing a resync on mailbox INBOX Failed";
  } else {
    copy_object(ns, box);

    uint32_t msg_count_org = mail_index_view_get_messages_count(box->view);
    i_debug("Message count before = %u", msg_count_org);
    EXPECT_EQ((uint32_t)1, msg_count_org);

    if (mailbox_sync(box, static_cast<mailbox_sync_flags>(MAILBOX_SYNC_FLAG_FORCE_RESYNC |
                                                          MAILBOX_SYNC_FLAG_FIX_INCONSISTENT)) < 0) {
      i_error("Forcing a resync on mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
      FAIL() << " Forcing a resync on mailbox INBOX Failed";
    }
    uint32_t msg_count = mail_index_view_get_messages_count(box->view);
    i_debug("Message count now = %u", msg_count);
    EXPECT_EQ((uint32_t)2, msg_count);
    uint32_t msg_count_new = msg_count_org + (uint32_t)1;
    EXPECT_EQ(msg_count, msg_count_new);
  }

  mailbox_free(&box);
}

TEST_F(SyncTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
