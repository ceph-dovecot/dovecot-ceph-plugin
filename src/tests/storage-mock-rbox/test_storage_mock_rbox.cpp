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
#include "mail-search-build.h"

#include "libdict-rados-plugin.h"
}
#include "dovecot-ceph-plugin-config.h"
#include "../test-utils/it_utils.h"

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
#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif

  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;

  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();
  // first call to open_connection will fail!
  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "user-rbox-test")).Times(AtLeast(1)).WillOnce(Return(-1));
  librmb::RadosMailObject *test_obj = new librmb::RadosMailObject();
  librmb::RadosMailObject *test_obj2 = new librmb::RadosMailObject();

  EXPECT_CALL(*storage_mock, alloc_mail_object()).WillOnce(Return(test_obj)).WillOnce(Return(test_obj2));

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
  EXPECT_EQ(test_obj->get_mail_buffer(), nullptr);
  EXPECT_EQ(test_obj2->get_mail_buffer(), nullptr);
  delete test_obj;
  delete test_obj2;
}

TEST_F(StorageTest, exec_write_op_fails) {
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

#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;
  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();
  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "user-rbox-test")).Times(AtLeast(1)).WillOnce(Return(0));
  EXPECT_CALL(*storage_mock, save_mail(_, _)).Times(1).WillOnce(Return(false));
  librmb::RadosMailObject *test_obj = new librmb::RadosMailObject();
  librmb::RadosMailObject *test_obj2 = new librmb::RadosMailObject();
  EXPECT_CALL(*storage_mock, alloc_mail_object()).Times(2).WillOnce(Return(test_obj)).WillOnce(Return(test_obj2));
  EXPECT_CALL(*storage_mock, free_mail_object(_)).Times(2);

  EXPECT_CALL(*storage_mock, set_metadata(_, _)).WillRepeatedly(Return(0));

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
  EXPECT_EQ(test_obj->get_mail_buffer(), NULL);
  EXPECT_EQ(test_obj2->get_mail_buffer(), NULL);

  delete test_obj;
  delete test_obj2;
}

TEST_F(StorageTest, write_op_fails) {
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

#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;
  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();

  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "user-rbox-test")).Times(AtLeast(1)).WillOnce(Return(0));
  EXPECT_CALL(*storage_mock, save_mail(_, _)).Times(1).WillOnce(Return(true));
  EXPECT_CALL(*storage_mock, wait_for_rados_operations(_)).Times(AtLeast(1)).WillRepeatedly(Return(true));
  librmb::RadosMailObject *test_obj = new librmb::RadosMailObject();
  librmb::RadosMailObject *test_obj2 = new librmb::RadosMailObject();
  EXPECT_CALL(*storage_mock, alloc_mail_object()).Times(2).WillOnce(Return(test_obj)).WillOnce(Return(test_obj2));
  EXPECT_CALL(*storage_mock, free_mail_object(_)).Times(2);

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
      FAIL() << "Saving should fail, due to connection to rados is not available.";
    } else if (mailbox_transaction_commit(&trans) < 0) {
      SUCCEED() << "should fail here";
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
    EXPECT_GE(ret, -1);
  }
  i_stream_unref(&input);
  mailbox_free(&box);
  EXPECT_EQ(test_obj->get_mail_buffer(), NULL);
  EXPECT_EQ(test_obj2->get_mail_buffer(), NULL);

  delete test_obj;
  delete test_obj2;
}


TEST_F(StorageTest, mock_copy_failed_due_to_rados_err) {
  struct mailbox_transaction_context *desttrans;
  struct mail_save_context *save_ctx;
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

  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();
  EXPECT_CALL(*storage_mock, save_mail(_, _)).Times(AtLeast(1)).WillRepeatedly(Return(true));
  EXPECT_CALL(*storage_mock, wait_for_rados_operations(_)).Times(AtLeast(1)).WillRepeatedly(Return(false));
  EXPECT_CALL(*storage_mock, set_metadata(_, _)).WillRepeatedly(Return(0));

  librmb::RadosMailObject *test_obj_save = new librmb::RadosMailObject();
  librmb::RadosMailObject *test_obj_save2 = new librmb::RadosMailObject();
  EXPECT_CALL(*storage_mock, alloc_mail_object())
      .Times(2)
      .WillOnce(Return(test_obj_save))
      .WillOnce(Return(test_obj_save2));

  // testdata
  testutils::ItUtils::add_mail(message, mailbox, StorageTest::s_test_mail_user->namespaces, storage_mock);

  delete test_obj_save;
  delete test_obj_save2;

  search_args = mail_search_build_init();
  sarg = mail_search_build_add(search_args, SEARCH_ALL);
  ASSERT_NE(sarg, nullptr);

  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);

  struct mailbox *box = mailbox_alloc(ns->list, mailbox, MAILBOX_FLAG_SAVEONLY);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;

  librmbtest::RadosStorageMock *storage_mock_copy = new librmbtest::RadosStorageMock();
  librmb::RadosMailObject *test_object = new librmb::RadosMailObject();
  librmb::RadosMailObject *test_object2 = new librmb::RadosMailObject();

  librmb::RadosMetadata recv_date = librmb::RadosMetadata(librmb::RBOX_METADATA_RECEIVED_TIME, time(NULL));
  test_object->add_metadata(recv_date);
  librmb::RadosMetadata guid = librmb::RadosMetadata(librmb::RBOX_METADATA_GUID, "67ffff24efc0e559194f00009c60b9f7");
  test_object->add_metadata(guid);

  EXPECT_CALL(*storage_mock_copy, alloc_mail_object())
      .Times(2)
      .WillOnce(Return(test_object))
      .WillOnce(Return(test_object2));
  EXPECT_CALL(*storage_mock_copy, wait_for_rados_operations(_)).Times(AtLeast(1)).WillRepeatedly(Return(false));
  EXPECT_CALL(*storage_mock_copy, set_metadata(_, _)).WillRepeatedly(Return(0));
  EXPECT_CALL(*storage_mock_copy, copy(_, _, _, _, _)).WillRepeatedly(Return(false));

  storage->s = storage_mock_copy;

  if (mailbox_open(box) < 0) {
    i_error("Opening mailbox %s failed: %s", mailbox, mailbox_get_last_internal_error(box, NULL));
    FAIL() << " Forcing a resync on mailbox INBOX Failed";
  }

#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  desttrans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  desttrans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif

  search_ctx = mailbox_search_init(desttrans, search_args, NULL, static_cast<mail_fetch_field>(0), NULL);
  mail_search_args_unref(&search_args);

  while (mailbox_search_next(search_ctx, &mail)) {
    save_ctx = mailbox_save_alloc(desttrans);  // src save context
    mailbox_save_copy_flags(save_ctx, mail);

    int ret2 = mailbox_copy(&save_ctx, mail);
    EXPECT_EQ(ret2, -1);
    break;  // only move one mail.
  }

  if (mailbox_search_deinit(&search_ctx) < 0) {
    FAIL() << "search deinit failed";
  }

  if (mailbox_transaction_commit(&desttrans) < 0) {
    SUCCEED() << "tnx commit failed";
  }
  mailbox_free(&box);
  EXPECT_EQ(test_object->get_mail_buffer(), NULL);
  EXPECT_EQ(test_object2->get_mail_buffer(), NULL);
  delete test_object;
  delete test_object2;
}


TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
