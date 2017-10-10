// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
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

using ::testing::AtLeast;
using ::testing::Return;

#pragma GCC diagnostic pop

#if DOVECOT_PREREQ(2, 3)
#define mailbox_get_last_internal_error(box, error_r) mailbox_get_last_internal_error(box, error_r)
#else
#define mailbox_get_last_internal_error(box, error_r) mailbox_get_last_error(box, error_r)
#endif

#ifndef i_zero
#define i_zero(p) memset(p, 0, sizeof(*(p)))
#endif



TEST_F(SyncTest, init) {}


static void add_mail(const char *message, const char *mailbox, struct mail_namespace *_ns) {
  struct mail_namespace *ns = mail_namespace_find_inbox(_ns);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, mailbox, (mailbox_flags)0);
  ASSERT_NE(box, nullptr);
  ASSERT_GE(mailbox_open(box), 0);


  /* const char *message =
       "From: user@domain.org\n"
       "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
       "Mime-Version: 1.0\n"
       "Content-Type: text/plain; charset=us-ascii\n"
       "\n"
       "body\n";*/

  struct istream *input = i_stream_create_from_data(message, strlen(message));

#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif

  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);
  ssize_t ret;
  bool save_failed = FALSE;

  if (mailbox_save_begin(&save_ctx, input) < 0) {
    i_error("Saving failed: %s", mailbox_get_last_internal_error(box, NULL));
    mailbox_transaction_rollback(&trans);
    FAIL() << "saving failed: " << mailbox_get_last_internal_error(box, NULL);
  } else {
    do {
      if (mailbox_save_continue(save_ctx) < 0) {
        save_failed = TRUE;
        ret = -1;
        FAIL() << "mailbox_save_continue() failed";
        break;
      }
    } while ((ret = i_stream_read(input)) > 0);
    EXPECT_EQ(ret, -1);

    if (input->stream_errno != 0) {
      FAIL() << "read(msg input) failed: " << i_stream_get_error(input);
    } else if (save_failed) {
      FAIL() << "Saving failed: " << mailbox_get_last_internal_error(box, NULL);
    } else if (mailbox_save_finish(&save_ctx) < 0) {
      FAIL() << "Saving failed: " << mailbox_get_last_internal_error(box, NULL);
    } else if (mailbox_transaction_commit(&trans) < 0) {
      FAIL() << "Save transaction commit failed: " << mailbox_get_last_internal_error(box, NULL);
    } else {
      ret = 0;
    }

    EXPECT_EQ(save_ctx, nullptr);
    if (save_ctx != nullptr)
      mailbox_save_cancel(&save_ctx);

    EXPECT_EQ(trans, nullptr);
    if (trans != nullptr)
      mailbox_transaction_rollback(&trans);

    EXPECT_TRUE(input->eof);
    EXPECT_GE(ret, 0);
  }
  i_stream_unref(&input);
  mailbox_free(&box);
}

static void copy_object(struct mail_namespace *_ns, struct mailbox *box) {
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  librmb::RadosMetadata xattr(librmb::rbox_metadata_key::RBOX_METADATA_ORIG_MAILBOX, box->name);
  librados::NObjectIterator iter = r_storage->s->find_mails(&xattr);

  std::string oid;
  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    oid = iter->get_oid();
    i_debug("copy : %s", oid.c_str());
    break;
  }

  guid_128_t temp_oid_guid;
  guid_128_generate(temp_oid_guid);

  std::string test_oid =  guid_128_to_string(temp_oid_guid);
  librados::ObjectWriteOperation write_op;
  librados::bufferlist list;
  list.append("100");
  write_op.copy_from(oid, r_storage->s->get_io_ctx(), r_storage->s->get_io_ctx().get_last_version());
  int ret = r_storage->s->get_io_ctx().operate(test_oid, &write_op);

  i_debug("copy aioperate: %d for %s", ret, test_oid.c_str());
  EXPECT_EQ(ret, 0);
  const char *metadata_name = "U";
  ret = r_storage->s->get_io_ctx().setxattr(test_oid, metadata_name, list);
  i_debug("copy operate setxattr: %d for %s", ret, test_oid.c_str());
  EXPECT_EQ(ret, 0);
}

TEST_F(SyncTest, force_resync_restore_missing_index_entry) {
  const char *message =
      "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  const char *mailbox = "INBOX";

  add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);
  add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);
  add_mail(message, mailbox, SyncTest::s_test_mail_user->namespaces);

  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);

  struct mailbox *box = mailbox_alloc(ns->list, mailbox, MAILBOX_FLAG_IGNORE_ACLS);

  if (mailbox_open(box) < 0) {
    i_error("Opening mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
    FAIL() << " Forcing a resync on mailbox INBOX Failed";
  } else {
    copy_object(ns, box);
    uint32_t msg_count_org = mail_index_view_get_messages_count(box->view);

    if (mailbox_sync(box, MAILBOX_SYNC_FLAG_FORCE_RESYNC | MAILBOX_SYNC_FLAG_FIX_INCONSISTENT) < 0) {
      i_error("Forcing a resync on mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
      FAIL() << " Forcing a resync on mailbox INBOX Failed";
    }
    uint32_t msg_count = mail_index_view_get_messages_count(box->view);

    EXPECT_EQ(msg_count, msg_count_org + 1);
  }

  mailbox_free(&box);
}

TEST_F(SyncTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
