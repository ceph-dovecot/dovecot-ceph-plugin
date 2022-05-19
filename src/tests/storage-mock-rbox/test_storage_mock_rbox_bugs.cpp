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
#include "rbox-save.h"

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

/**
 * Error test:
 *
 * - save mail to rados fails.
 *
 */
/* TEST_F(StorageTest, save_mail_rados_connection_failed) {
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

  EXPECT_CALL(*storage_mock, set_ceph_wait_method(_)).Times(1);

  EXPECT_CALL(*storage_mock, set_namespace(_)).Times(1);
  EXPECT_CALL(*storage_mock, get_namespace()).Times(0);

  EXPECT_CALL(*storage_mock, delete_mail(Matcher<librmb::RadosMail*>(_))).Times(1);

  EXPECT_CALL(*storage_mock, close_connection()).Times(0);

  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "ceph", "client.admin"))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*storage_mock, get_max_object_size())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(100000));

  EXPECT_CALL(*storage_mock, get_max_write_size_bytes())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(10));

  EXPECT_CALL(*storage_mock, save_mail(Matcher<librados::ObjectWriteOperation *>(_), _)).Times(1).WillOnce(Return(false));

  librmb::RadosMail *test_obj = new librmb::RadosMail();
  test_obj->set_mail_buffer(nullptr);
  librmb::RadosMail *test_obj2 = new librmb::RadosMail();
  test_obj2->set_mail_buffer(nullptr);
  EXPECT_CALL(*storage_mock, alloc_rados_mail()).Times(2).WillOnce(Return(test_obj)).WillOnce(Return(test_obj2));
  EXPECT_CALL(*storage_mock, free_rados_mail(_)).Times(2);

  delete storage->ms;
  librmbtest::RadosMetadataStorageProducerMock *ms_p_mock = new librmbtest::RadosMetadataStorageProducerMock();
  storage->ms = ms_p_mock;

  librmbtest::RadosStorageMetadataMock ms_mock;
  EXPECT_CALL(*ms_p_mock, get_storage()).WillRepeatedly(Return(&ms_mock));
  EXPECT_CALL(ms_mock, set_metadata(_, _,_)).WillRepeatedly(Return(0));
  EXPECT_CALL(*ms_p_mock, create_metadata_storage(_,_)).Times(1);

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
  EXPECT_CALL(*cfg_mock, is_config_valid()).WillRepeatedly(Return(true));
  EXPECT_CALL(*cfg_mock, is_write_chunks()).WillRepeatedly(Return(false));
  EXPECT_CALL(*cfg_mock, is_ceph_posix_bugfix_enabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(*cfg_mock, is_ceph_aio_wait_for_safe_and_cb()).WillOnce(Return(false));
  EXPECT_CALL(*cfg_mock, load_rados_config()).WillOnce(Return(0));
  EXPECT_CALL(*cfg_mock, is_mail_attribute(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*cfg_mock, is_user_mapping()).WillRepeatedly(Return(false));

  storage->ns_mgr->set_config(cfg_mock);

  storage->config = cfg_mock;
  storage->s = storage_mock;

  bool save_failed = FALSE;
  // ------ begin flow 

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
      mailbox_transaction_rollback(&trans);

    } else if (mailbox_transaction_commit(&trans) < 0) {
      FAIL() << "Save transaction commit failed: " << mailbox_get_last_internal_error(box, NULL);
    } else {
      ret = 0;
    }
  
    i_info("ok done.");
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
 */
/**
 * Error test:
 *
 * - save mail to rados fails.
 *
 */
TEST_F(StorageTest, save_mail_success) {
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

  EXPECT_CALL(*storage_mock, aio_operate(_,_,_,_)).Times(AtLeast(1)).WillRepeatedly(Return(0));
  EXPECT_CALL(*storage_mock, wait_for_write_operations_complete(_,_)).WillRepeatedly(Return(false));//failed = false
  librados::IoCtx io_ctx;

  EXPECT_CALL(*storage_mock, get_io_ctx()).WillRepeatedly(ReturnRef(io_ctx));

  EXPECT_CALL(*storage_mock, set_ceph_wait_method(_)).Times(1);

  EXPECT_CALL(*storage_mock, set_namespace(_)).Times(1);
  EXPECT_CALL(*storage_mock, get_namespace()).Times(0);

  EXPECT_CALL(*storage_mock, delete_mail(Matcher<librmb::RadosMail*>(_))).Times(0);

  EXPECT_CALL(*storage_mock, close_connection()).Times(1);

  EXPECT_CALL(*storage_mock, open_connection("mail_storage", "ceph", "client.admin"))
      .Times(AtLeast(1))
      .WillRepeatedly(Return(0));

  EXPECT_CALL(*storage_mock, get_max_object_size())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(100000));

  EXPECT_CALL(*storage_mock, get_max_write_size_bytes())
      .Times(AtLeast(1))
      .WillRepeatedly(Return(10));
 
  
  librmb::RadosMail *test_obj = new librmb::RadosMail();
  test_obj->set_mail_buffer(nullptr);
  librmb::RadosMail *test_obj2 = new librmb::RadosMail();
  test_obj2->set_mail_buffer(nullptr);
  EXPECT_CALL(*storage_mock, alloc_rados_mail()).Times(2).WillOnce(Return(test_obj)).WillOnce(Return(test_obj2));
  EXPECT_CALL(*storage_mock, free_rados_mail(_)).Times(2);

  delete storage->ms;
  librmbtest::RadosMetadataStorageProducerMock *ms_p_mock = new librmbtest::RadosMetadataStorageProducerMock();
  storage->ms = ms_p_mock;

  librmbtest::RadosStorageMetadataMock ms_mock;
  EXPECT_CALL(*ms_p_mock, get_storage()).WillRepeatedly(Return(&ms_mock));
  EXPECT_CALL(ms_mock, set_metadata(_, _,_)).WillRepeatedly(Return(0));
  EXPECT_CALL(*ms_p_mock, create_metadata_storage(_,_)).Times(1);

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
  EXPECT_CALL(*cfg_mock, is_config_valid()).WillRepeatedly(Return(true));
  EXPECT_CALL(*cfg_mock, is_write_chunks()).WillRepeatedly(Return(false));
  EXPECT_CALL(*cfg_mock, is_ceph_posix_bugfix_enabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(*cfg_mock, is_ceph_aio_wait_for_safe_and_cb()).WillOnce(Return(false));
  EXPECT_CALL(*cfg_mock, load_rados_config()).WillOnce(Return(0));
  EXPECT_CALL(*cfg_mock, is_mail_attribute(_)).WillRepeatedly(Return(true));
  EXPECT_CALL(*cfg_mock, get_chunk_size()).WillOnce(Return(100));
  EXPECT_CALL(*cfg_mock, is_user_mapping()).WillRepeatedly(Return(false));
  EXPECT_CALL(*cfg_mock, get_write_method()).WillRepeatedly(Return(1));
  

  storage->ns_mgr->set_config(cfg_mock);

  storage->config = cfg_mock;
  storage->s = storage_mock;

  bool save_failed = FALSE;
  // ------ begin flow 

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
      FAIL() << "Save transaction commit failed: " << mailbox_get_last_internal_error(box, NULL);
    } else {
      ret = 0;
    }
  
    i_info("ok done.");
    EXPECT_GE(ret, 0);
  }
  i_stream_unref(&input);
  mailbox_free(&box);
i_info("mailbox free done");
  
  i_info("next delete test_obj");
  delete test_obj;
  if (test_obj2->get_mail_buffer() != nullptr) {
    delete test_obj2->get_mail_buffer();
  }
  i_info("next delete test_obj2");
  delete test_obj2;

}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
