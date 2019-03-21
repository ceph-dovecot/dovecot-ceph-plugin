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
#include "ostream.h"

#include "libdict-rados-plugin.h"
}
#include "dovecot-ceph-plugin-config.h"
#include "../test-utils/it_utils.h"

#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"
#include "rados-dovecot-ceph-cfg-impl.h"
#include "../../storage-rbox/istream-bufferlist.h"
#include "../../storage-rbox/ostream-bufferlist.h"
using ::testing::_;
using ::testing::AtLeast;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::ReturnRef;
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
/**
 * Error test:
 * - open_connection to rados will fail with -1 .
 */
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
  memset(reason, '\0', sizeof(reason));
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif

  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;

  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();
  // first call to open_connection will fail!
  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "ceph", "client.admin"))
      .Times(AtLeast(1))
      .WillOnce(Return(-1));

  librmb::RadosMail *test_obj = new librmb::RadosMail();

  EXPECT_CALL(*storage_mock, alloc_rados_mail()).WillOnce(Return(test_obj));

  delete storage->ms;
  librmbtest::RadosMetadataStorageProducerMock *storage_ms_mock = new librmbtest::RadosMetadataStorageProducerMock();
  librmbtest::RadosStorageMetadataMock mod;
  EXPECT_CALL(*storage_ms_mock, create_metadata_storage(_, _)).WillRepeatedly(Return(&mod));
  storage->ms = storage_ms_mock;

  // storage->ns_mgr->set_storage(storage_mock);
  storage->s = storage_mock;

  bool save_failed = FALSE;

  if (mailbox_save_begin(&save_ctx, input) < 0) {
    i_error("Saving failed: %s", mailbox_get_last_internal_error(box, NULL));
    mailbox_transaction_rollback(&trans);
    SUCCEED() << "saving failed: " << mailbox_get_last_internal_error(box, NULL);
  } else {
    ssize_t ret;
    do {
      if (mailbox_save_continue(save_ctx) < 0) {
        save_failed = TRUE;
        ret = -1;
        SUCCEED() << "mailbox_save_continue() failed";
        break;
      }
    } while ((ret = i_stream_read(input)) > 0);
    EXPECT_EQ(ret, -1);

    if (input->stream_errno != 0) {
      FAIL() << "read(msg input) failed: " << i_stream_get_error(input);
    } else if (save_failed) {
      SUCCEED() << "Saving failed: " << mailbox_get_last_internal_error(box, NULL);
    } else if (mailbox_save_finish(&save_ctx) < 0) {
      FAIL() << "Saving should fail, due to connection to rados is not available.";
    } else if (mailbox_transaction_commit(&trans) < 0) {
      FAIL() << "Save transaction commit failed: " << mailbox_get_last_internal_error(box, NULL);
    } else {
      ret = 0;
    }

    EXPECT_NE(save_ctx, nullptr);
    if (save_ctx != nullptr)
      mailbox_save_cancel(&save_ctx);

    EXPECT_NE(trans, nullptr);
    if (trans != nullptr)
      mailbox_transaction_rollback(&trans);

    EXPECT_FALSE(input->eof);
    EXPECT_GE(ret, -1);
  }
  i_stream_unref(&input);
  mailbox_free(&box);

  delete test_obj;
}

/**
 * Error test:
 *
 * - save mail to rados fails.
 *
 */
TEST_F(StorageTest, save_mail_fail_test) {
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
  memset(reason, '\0', sizeof(reason));
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;
  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();
  librados::IoCtx test_ioctx;
  EXPECT_CALL(*storage_mock, get_io_ctx()).WillRepeatedly(ReturnRef(test_ioctx));

  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "ceph", "client.admin"))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(0));
  EXPECT_CALL(*storage_mock, save_mail(_, _, Matcher<bool>(_))).Times(1).WillOnce(Return(false));

  librmb::RadosMail *test_obj = new librmb::RadosMail();
  test_obj->set_mail_buffer(nullptr);
  librmb::RadosMail *test_obj2 = new librmb::RadosMail();
  test_obj2->set_mail_buffer(nullptr);
  EXPECT_CALL(*storage_mock, alloc_rados_mail()).Times(2).WillOnce(Return(test_obj)).WillOnce(Return(test_obj2));
  EXPECT_CALL(*storage_mock, free_rados_mail(_)).Times(2);

  delete storage->ms;
  librmbtest::RadosMetadataStorageProducerMock *ms_p_mock = new librmbtest::RadosMetadataStorageProducerMock();
  librmbtest::RadosStorageMetadataMock mod;
  EXPECT_CALL(*ms_p_mock, create_metadata_storage(_, _)).WillRepeatedly(Return(&mod));
  storage->ms = ms_p_mock;

  librmbtest::RadosStorageMetadataMock ms_mock;
  EXPECT_CALL(*ms_p_mock, get_storage()).WillRepeatedly(Return(&ms_mock));
  EXPECT_CALL(ms_mock, set_metadata(_, _)).WillRepeatedly(Return(0));

  delete storage->config;
  librmbtest::RadosDovecotCephCfgMock *cfg_mock = new librmbtest::RadosDovecotCephCfgMock();
  EXPECT_CALL(*cfg_mock, is_config_valid()).WillRepeatedly(Return(true));
  std::string user = "client.admin";
  std::string cluster = "ceph";
  std::string pool = "mail_storage";
  std::string suffix = "_u";

  EXPECT_CALL(*cfg_mock, get_rados_username()).WillRepeatedly(ReturnRef(user));
  EXPECT_CALL(*cfg_mock, get_rados_cluster_name()).WillRepeatedly(ReturnRef(cluster));
  EXPECT_CALL(*cfg_mock, get_pool_name()).WillRepeatedly(ReturnRef(pool));
  EXPECT_CALL(*cfg_mock, get_user_suffix()).WillRepeatedly(ReturnRef(suffix));
  storage->ns_mgr->set_config(cfg_mock);

  storage->config = cfg_mock;
  storage->s = storage_mock;

  bool save_failed = FALSE;

  if (mailbox_save_begin(&save_ctx, input) < 0) {
    i_error("Saving failed: %s", mailbox_get_last_internal_error(box, NULL));
    mailbox_transaction_rollback(&trans);
    FAIL() << "saving failed: " << mailbox_get_last_internal_error(box, NULL);
  } else {
    ssize_t ret;
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

  if (test_obj->get_mail_buffer() != nullptr) {
    delete test_obj->get_mail_buffer();
  }
  delete test_obj;
  if (test_obj2->get_mail_buffer() != nullptr) {
    delete test_obj2->get_mail_buffer();
  }
  delete test_obj2;
}
/**
 * Error test:
 *
 * - save mail to rados fails in mailbox_transaction_commit
 *
 */
TEST_F(StorageTest, write_op_fails) {
  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", (mailbox_flags)0);
  ASSERT_NE(box, nullptr);
  i_debug("preparing to open");
  ASSERT_GE(mailbox_open(box), 0);
  i_debug("mailbox open");
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
  memset(reason, '\0', sizeof(reason));
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);
  // set the Mock storage
  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;

  librmbtest::RadosStorageMock *storage_mock = new librmbtest::RadosStorageMock();
  librados::IoCtx test_ioctx;
  EXPECT_CALL(*storage_mock, get_io_ctx()).WillRepeatedly(ReturnRef(test_ioctx));

  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "ceph", "client.admin"))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(0));

  // simulate wait_for_rados_operations fail
  EXPECT_CALL(*storage_mock, wait_for_rados_operations(_))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(true));  // will set ctx->failed to true

  EXPECT_CALL(*storage_mock, save_mail(Matcher<librados::ObjectWriteOperation *>(_), _, _))
      .WillRepeatedly(Return(true));

  EXPECT_CALL(*storage_mock, read_mail(_, _)).WillRepeatedly(Return(-2));

  librmb::RadosMail *test_obj = new librmb::RadosMail();
  test_obj->set_mail_buffer(nullptr);
  librmb::RadosMail *test_obj2 = new librmb::RadosMail();
  test_obj2->set_mail_buffer(nullptr);
  EXPECT_CALL(*storage_mock, alloc_rados_mail()).Times(2).WillOnce(Return(test_obj)).WillOnce(Return(test_obj2));

  EXPECT_CALL(*storage_mock, free_rados_mail(_)).Times(2);
  delete storage->config;
  librmbtest::RadosDovecotCephCfgMock *cfg_mock = new librmbtest::RadosDovecotCephCfgMock();
  EXPECT_CALL(*cfg_mock, is_config_valid()).WillRepeatedly(Return(true));
  std::string user = "client.admin";
  std::string cluster = "ceph";
  std::string pool = "mail_storage";
  std::string suffix = "_u";

  EXPECT_CALL(*cfg_mock, get_rados_username()).WillRepeatedly(ReturnRef(user));
  EXPECT_CALL(*cfg_mock, get_rados_cluster_name()).WillRepeatedly(ReturnRef(cluster));
  EXPECT_CALL(*cfg_mock, get_pool_name()).WillRepeatedly(ReturnRef(pool));
  EXPECT_CALL(*cfg_mock, get_user_suffix()).WillRepeatedly(ReturnRef(suffix));
  storage->ns_mgr->set_config(cfg_mock);

  storage->config = cfg_mock;
  storage->s = storage_mock;

  delete storage->ms;
  librmbtest::RadosMetadataStorageProducerMock *ms_p_mock = new librmbtest::RadosMetadataStorageProducerMock();
  librmbtest::RadosStorageMetadataMock mod;
  EXPECT_CALL(*ms_p_mock, create_metadata_storage(_, _)).WillRepeatedly(Return(&mod));
  storage->ms = ms_p_mock;

  librmbtest::RadosStorageMetadataMock ms_mock;
  EXPECT_CALL(*ms_p_mock, get_storage()).WillRepeatedly(Return(&ms_mock));
  EXPECT_CALL(ms_mock, set_metadata(_, _)).WillRepeatedly(Return(0));

  bool save_failed = FALSE;

  if (mailbox_save_begin(&save_ctx, input) < 0) {
    i_error("Saving failed: %s", mailbox_get_last_internal_error(box, NULL));
    mailbox_transaction_rollback(&trans);
    FAIL() << "saving failed: " << mailbox_get_last_internal_error(box, NULL);
  } else {
    ssize_t ret;
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
      i_debug("failed at correct place");
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

  if (test_obj->get_mail_buffer() != nullptr) {
    delete test_obj->get_mail_buffer();
  }
  delete test_obj;
  if (test_obj2->get_mail_buffer() != nullptr) {
    delete test_obj2->get_mail_buffer();
  }
  delete test_obj2;
}
/**
 * Error test:
 *
 * - copy mail fails due to storage.copy call returns -1
 */
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

  EXPECT_CALL(*storage_mock, wait_for_rados_operations(_)).Times(AtLeast(1)).WillRepeatedly(Return(false));
  librados::IoCtx test_ioctx;
  EXPECT_CALL(*storage_mock, get_io_ctx()).WillRepeatedly(ReturnRef(test_ioctx));
  EXPECT_CALL(*storage_mock, save_mail(Matcher<librados::ObjectWriteOperation *>(_), _, _))
      .WillRepeatedly(Return(true));

  librmb::RadosMail *test_obj_save = new librmb::RadosMail();
  librmb::RadosMail *test_obj_save2 = new librmb::RadosMail();
  test_obj_save->set_mail_buffer(nullptr);
  test_obj_save2->set_mail_buffer(nullptr);

  EXPECT_CALL(*storage_mock, alloc_rados_mail())
      .Times(2)
      .WillOnce(Return(test_obj_save))
      .WillOnce(Return(test_obj_save2));

  // testdata
  testutils::ItUtils::add_mail(message, mailbox, StorageTest::s_test_mail_user->namespaces, storage_mock);

  if (test_obj_save->get_mail_buffer() != nullptr) {
    delete test_obj_save->get_mail_buffer();
  }
  delete test_obj_save;
  if (test_obj_save2->get_mail_buffer() != nullptr) {
    delete test_obj_save2->get_mail_buffer();
  }
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
  librmb::RadosMail *test_object = new librmb::RadosMail();
  librmb::RadosMail *test_object2 = new librmb::RadosMail();
  test_object->set_mail_buffer(nullptr);
  test_object2->set_mail_buffer(nullptr);

  librmb::RadosMetadata recv_date = librmb::RadosMetadata(librmb::RBOX_METADATA_RECEIVED_TIME, time(NULL));
  test_object->add_metadata(recv_date);
  librmb::RadosMetadata guid = librmb::RadosMetadata(librmb::RBOX_METADATA_GUID, "67ffff24efc0e559194f00009c60b9f7");
  test_object->add_metadata(guid);

  EXPECT_CALL(*storage_mock_copy, alloc_rados_mail())
      .Times(2)
      .WillOnce(Return(test_object))
      .WillOnce(Return(test_object2));
  EXPECT_CALL(*storage_mock_copy, wait_for_rados_operations(_)).Times(AtLeast(1)).WillRepeatedly(Return(false));

  EXPECT_CALL(*storage_mock_copy, copy(_, _, _, _, _)).WillRepeatedly(Return(-1));
  EXPECT_CALL(*storage_mock_copy, get_io_ctx()).WillRepeatedly(ReturnRef(test_ioctx));

  storage->s = storage_mock_copy;
  delete storage->config;
  librmbtest::RadosDovecotCephCfgMock *cfg_mock = new librmbtest::RadosDovecotCephCfgMock();
  EXPECT_CALL(*cfg_mock, is_config_valid()).WillRepeatedly(Return(true));
  std::string user = "client.admin";
  std::string cluster = "ceph";
  std::string pool = "mail_storage";
  std::string suffix = "_u";

  delete storage->ms;
  librmbtest::RadosMetadataStorageProducerMock *ms_p_mock = new librmbtest::RadosMetadataStorageProducerMock();
  librmbtest::RadosStorageMetadataMock mod;
  EXPECT_CALL(*ms_p_mock, create_metadata_storage(_, _)).WillRepeatedly(Return(&mod));
  storage->ms = ms_p_mock;

  librmbtest::RadosStorageMetadataMock ms_mock;
  EXPECT_CALL(*ms_p_mock, get_storage()).WillRepeatedly(Return(&ms_mock));
  EXPECT_CALL(ms_mock, set_metadata(_, _)).WillRepeatedly(Return(0));

  EXPECT_CALL(*cfg_mock, get_rados_username()).WillRepeatedly(ReturnRef(user));
  EXPECT_CALL(*cfg_mock, get_rados_cluster_name()).WillRepeatedly(ReturnRef(cluster));
  EXPECT_CALL(*cfg_mock, get_pool_name()).WillRepeatedly(ReturnRef(pool));
  EXPECT_CALL(*cfg_mock, get_user_suffix()).WillRepeatedly(ReturnRef(suffix));
  storage->ns_mgr->set_config(cfg_mock);

  storage->config = cfg_mock;

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
  int ret2 = 0;
  while (mailbox_search_next(search_ctx, &mail)) {
    save_ctx = mailbox_save_alloc(desttrans);  // src save context
    mailbox_save_copy_flags(save_ctx, mail);

    ret2 = mailbox_copy(&save_ctx, mail);

    break;  // only move one mail.
  }

  if (mailbox_search_deinit(&search_ctx) < 0) {
    i_debug("search deint failed!");
  }

  if (mailbox_transaction_commit(&desttrans) < 0) {
    i_debug("transaction commit <0");
    SUCCEED() << "tnx commit failed";
  }

  // mail should be marked as expunged!!!
  EXPECT_EQ(ret2, -1);
  mailbox_free(&box);

  if (test_object->get_mail_buffer() != nullptr) {
    delete test_object->get_mail_buffer();
  }
  delete test_object;
  if (test_object2->get_mail_buffer() != nullptr) {
    delete test_object2->get_mail_buffer();
  }
  delete test_object2;
}
/**
 * Error test:
 *
 * - eval copy from input to output stream
 */
TEST_F(StorageTest, copy_input_to_output_stream) {
  librados::bufferlist *buffer = new librados::bufferlist();
  // librados::bufferlist buffer_out;
  librmb::RadosMail mail;
  buffer->append("\r\t\0\nJAN");
  unsigned long physical_size = buffer->length();
  struct istream *input;  // = *stream_r;
  struct ostream *output;

  librados::bufferlist buffer2;
  mail.set_mail_buffer(&buffer2);
  output = o_stream_create_bufferlist(&mail, nullptr, false);
  input = i_stream_create_from_bufferlist(buffer, physical_size);

  do {
    if (o_stream_send_istream(output, input) < 0) {
      EXPECT_EQ(1, -1);
    }

  } while (i_stream_read(input) > 0);

  EXPECT_EQ(buffer->to_str(), mail.get_mail_buffer()->to_str());
  o_stream_unref(&output);
  i_stream_unref(&input);
}
/*
TEST_F(StorageTest, eval_output_append) {
  librados::bufferlist buffer;
  librados::bufferlist buffer_out;


  struct ostream *output;
  output = o_stream_create_bufferlist(&buffer_out);
  std::string toappend = "def";
  o_stream_buffer_write_at(output->real_stream, reinterpret_cast<const void *>(toappend.c_str()), toappend.length(), 0);
  EXPECT_EQ(toappend, buffer_out.to_str());
  std::string toappend2 = "abc";
  o_stream_buffer_write_at(output->real_stream, reinterpret_cast<const void *>(toappend2.c_str()), toappend2.length(),
                           0);
  EXPECT_EQ("abcdef", buffer_out.to_str());
  std::string toapend3 = "ghjk";
  o_stream_buffer_write_at(output->real_stream, reinterpret_cast<const void *>(toapend3.c_str()), toapend3.length(), 6);
  EXPECT_EQ("abcdefghjk", buffer_out.to_str());
  std::string toapend4 = "i";
  o_stream_buffer_write_at(output->real_stream, reinterpret_cast<const void *>(toapend4.c_str()), toapend4.length(), 8);
  EXPECT_EQ("abcdefghijk", buffer_out.to_str());
  o_stream_unref(&output);
}
*/
TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
