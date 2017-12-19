/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#include "../test-utils/it_utils.h"

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "../mocks/mock_test.h"

using ::testing::AtLeast;
using ::testing::Return;
using ::testing::_;
using ::testing::Matcher;
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

namespace testutils {

ItUtils::ItUtils() {}

ItUtils::~ItUtils() {}
void ItUtils::add_mail(const char *message, const char *mailbox, struct mail_namespace *_ns,
                       librmb::RadosStorage *storage_impl) {
  struct mail_namespace *ns = mail_namespace_find_inbox(_ns);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, mailbox, (mailbox_flags)0);
  ASSERT_NE(box, nullptr);
  ASSERT_GE(mailbox_open(box), 0);

  struct istream *input = i_stream_create_from_data(message, strlen(message));

#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  memset(reason, '\0', sizeof(reason));
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  struct rbox_storage *storage = (struct rbox_storage *)box->storage;
  delete storage->s;
  storage->s = storage_impl;
  storage->ns_mgr->set_config(storage->config);

  delete storage->config;
  librmbtest::RadosDovecotCephCfgMock *cfg_mock = new librmbtest::RadosDovecotCephCfgMock();
  EXPECT_CALL(*cfg_mock, is_config_valid()).WillRepeatedly(Return(true));
  std::string user = "client.admin";
  std::string cluster = "ceph";
  std::string pool = "mail_storage";
  std::string suffix = "_u";

  EXPECT_CALL(*cfg_mock, get_rados_username()).WillRepeatedly(ReturnRef(user));
  EXPECT_CALL(*cfg_mock, get_rados_cluster_name()).WillRepeatedly(ReturnRef(cluster));
  EXPECT_CALL(*cfg_mock, get_pool_name()).WillRepeatedly(Return(pool));
  EXPECT_CALL(*cfg_mock, get_user_suffix()).WillRepeatedly(Return(suffix));
  storage->ns_mgr->set_config(cfg_mock);
  storage->config = cfg_mock;
  ItUtils::add_mail(save_ctx, input, box, trans);

  i_stream_unref(&input);
  mailbox_free(&box);
}
void ItUtils::add_mail(const char *message, const char *mailbox, struct mail_namespace *_ns) {
  struct mail_namespace *ns = mail_namespace_find_inbox(_ns);
  ASSERT_NE(ns, nullptr);
  struct mailbox *box = mailbox_alloc(ns->list, mailbox, (mailbox_flags)0);
  ASSERT_NE(box, nullptr);
  ASSERT_GE(mailbox_open(box), 0);

  struct istream *input = i_stream_create_from_data(message, strlen(message));

#ifdef DOVECOT_CEPH_PLUGIN_HAVE_MAIL_STORAGE_TRANSACTION_OLD_SIGNATURE
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL);
#else
  char reason[256];
  memset(reason, '\0', sizeof(reason));
  struct mailbox_transaction_context *trans = mailbox_transaction_begin(box, MAILBOX_TRANSACTION_FLAG_EXTERNAL, reason);
#endif
  struct mail_save_context *save_ctx = mailbox_save_alloc(trans);

  ItUtils::add_mail(save_ctx, input, box, trans);

  i_stream_unref(&input);
  mailbox_free(&box);
}

void ItUtils::add_mail(struct mail_save_context *save_ctx, struct istream *input, struct mailbox *box,
                       struct mailbox_transaction_context *trans) {
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
}

} /* namespace testutils */
