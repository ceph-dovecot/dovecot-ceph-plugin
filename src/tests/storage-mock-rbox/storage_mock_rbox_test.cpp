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

#include "../storage-mock-rbox/TestCase.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

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

#include "libdict-rados-plugin.h"
}
#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::_;

#pragma GCC diagnostic pop

#if DOVECOT_PREREQ(2, 3)
#define mailbox_get_last_internal_error(box, error_r) mailbox_get_last_internal_error(box, error_r)
#else
#define mailbox_get_last_internal_error(box, error_r) mailbox_get_last_error(box, error_r)
#endif

#ifndef i_zero
#define i_zero(p) memset(p, 0, sizeof(*(p)))
#endif

TEST_F(StorageTest, init) {}

TEST_F(StorageTest, mail_save_to_inbox_storage_mock_no_rados_available) {
  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", (mailbox_flags)0);
  ASSERT_NE(box, nullptr);
  ASSERT_GE(mailbox_open(box), 0);

  const char *message =
      "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  struct istream *input = i_stream_create_from_data(message, strlen(message));

  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;
  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();
  // first call to open_connection will fail!
  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "user-rbox-test")).Times(AtLeast(1)).WillOnce(Return(-1));

  storage->s = storage_mock;
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
      SUCCEED() << "Saving should fail, due to connection to rados is not available.";
    } else if (mailbox_transaction_commit(&trans) < 0) {
      FAIL() << "Save transaction commit failed: " << mailbox_get_last_internal_error(box, NULL);
    } else {
      ret = 0;
      // FAIL() << "connection is not open: " << mailbox_get_last_internal_error(box, NULL);
    }

    EXPECT_EQ(save_ctx, nullptr);
    if (save_ctx != nullptr)
      mailbox_save_cancel(&save_ctx);

    EXPECT_NE(trans, nullptr);
    if (trans != nullptr)
      mailbox_transaction_rollback(&trans);

    EXPECT_TRUE(input->eof);
    EXPECT_GE(ret, -1);
  }
  i_stream_unref(&input);
  mailbox_free(&box);
}

TEST_F(StorageTest, exec_write_op_fails) {
  struct mail_namespace *ns = mail_namespace_find_inbox(
      s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", (mailbox_flags) 0);
  ASSERT_NE(box, nullptr);
  ASSERT_GE(mailbox_open(box), 0);

  const char *message = "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  struct istream *input = i_stream_create_from_data(message, strlen(message));

  struct mailbox_transaction_context *trans = mailbox_transaction_begin(
      box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *) box->storage;
  delete storage->s;
  librmbtest::RadosStorageMock *storage_mock =
      new librmbtest::RadosStorageMock();
  // first call to open_connection will fail!
  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "user-rbox-test")).
      Times(AtLeast(1)).WillOnce(Return(0));

  EXPECT_CALL(*storage_mock,get_max_write_size_bytes()).Times(AtLeast(1))
      .WillOnce(Return(10));
  EXPECT_CALL(*storage_mock, split_buffer_and_exec_op(_, _, _, _, _))
      .Times(
      AtLeast(1)).WillRepeatedly(Return(-1));

  storage->s = storage_mock;
  ssize_t ret;

  bool save_failed = FALSE;

  if (mailbox_save_begin(&save_ctx, input) < 0) {
    i_error("Saving failed: %s", mailbox_get_last_internal_error(box, NULL));
    mailbox_transaction_rollback(&trans);
    FAIL()<< "saving failed: " << mailbox_get_last_internal_error(box, NULL);
  } else {
    do {
      if (mailbox_save_continue(save_ctx) < 0) {
        save_failed = TRUE;
        ret = -1;
        FAIL() << "mailbox_save_continue() failed";
        break;
      }
    }while ((ret = i_stream_read(input)) > 0);
    EXPECT_EQ(ret, -1);

    if (input->stream_errno != 0) {
      FAIL() << "read(msg input) failed: " << i_stream_get_error(input);
    } else if (save_failed) {
      FAIL() << "Saving failed: " << mailbox_get_last_internal_error(box, NULL);
    } else if (mailbox_save_finish(&save_ctx) < 0) {
      SUCCEED() << "Saving should fail, due to connection to rados is not available.";
    } else if (mailbox_transaction_commit(&trans) < 0) {
      FAIL() << "Save transaction commit failed: " << mailbox_get_last_internal_error(box, NULL);
    } else {
      ret = 0;
      // FAIL() << "connection is not open: " << mailbox_get_last_internal_error(box, NULL);
    }

    EXPECT_EQ(save_ctx, nullptr);
    if (save_ctx != nullptr)
    mailbox_save_cancel(&save_ctx);

    EXPECT_NE(trans, nullptr);
    if (trans != nullptr)
    mailbox_transaction_rollback(&trans);

    EXPECT_TRUE(input->eof);
    EXPECT_GE(ret, -1);
  }
  mailbox_free(&box);
}

TEST_F(StorageTest, write_op_fails) {
  struct mail_namespace *ns = mail_namespace_find_inbox(
      s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", (mailbox_flags) 0);
  ASSERT_NE(box, nullptr);
  ASSERT_GE(mailbox_open(box), 0);

  const char *message = "From: user@domain.org\n"
      "Date: Sat, 24 Mar 2017 23:00:00 +0200\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "body\n";

  struct istream *input = i_stream_create_from_data(message, strlen(message));

  struct mailbox_transaction_context *trans = mailbox_transaction_begin(
      box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *) box->storage;
  delete storage->s;
  librmbtest::RadosStorageMock *storage_mock =
      new librmbtest::RadosStorageMock();
  // first call to open_connection will fail!
  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "user-rbox-test")).
      Times(AtLeast(1)).WillOnce(Return(0));


  EXPECT_CALL(*storage_mock,get_max_write_size_bytes()).Times(AtLeast(1))
      .WillOnce(Return(10));

  EXPECT_CALL(*storage_mock,wait_for_write_operations_complete(_)).Times(
      AtLeast(1)).WillOnce(Return(true)
  );

  storage->s = storage_mock;
  ssize_t ret;

  bool save_failed = FALSE;

  if (mailbox_save_begin(&save_ctx, input) < 0) {
    i_error("Saving failed: %s", mailbox_get_last_internal_error(box, NULL));
    mailbox_transaction_rollback(&trans);
    FAIL()<< "saving failed: " << mailbox_get_last_internal_error(box, NULL);
  } else {
    do {
      if (mailbox_save_continue(save_ctx) < 0) {
        save_failed = TRUE;
        ret = -1;
        FAIL() << "mailbox_save_continue() failed";
        break;
      }
    }while ((ret = i_stream_read(input)) > 0);
    EXPECT_EQ(ret, -1);

    if (input->stream_errno != 0) {
      FAIL() << "read(msg input) failed: " << i_stream_get_error(input);
    } else if (save_failed) {
      FAIL() << "Saving failed: " << mailbox_get_last_internal_error(box, NULL);
    } else if (mailbox_save_finish(&save_ctx) < 0) {
      FAIL() << "Saving should fail, due to connection to rados is not available.";
    } else if (mailbox_transaction_commit(&trans) < 0) {
      SUCCEED() << "should fail here";
    } else {
      ret = 0;
      // FAIL() << "connection is not open: " << mailbox_get_last_internal_error(box, NULL);
    }

    EXPECT_EQ(save_ctx, nullptr);
    if (save_ctx != nullptr)
    mailbox_save_cancel(&save_ctx);

    EXPECT_EQ(trans, nullptr);
    if (trans != nullptr)
    mailbox_transaction_rollback(&trans);

    EXPECT_TRUE(input->eof);
    EXPECT_GE(ret, -1);
  }
  mailbox_free(&box);
}

TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
