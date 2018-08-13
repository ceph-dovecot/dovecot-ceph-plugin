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
#include "rbox-mail.h"
#include "rbox-storage.h"
#include "rados-util.h"

using ::testing::AtLeast;
using ::testing::Return;

TEST_F(StorageTest, init) {}

TEST_F(StorageTest, mailbox_open_inbox) {
  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", MAILBOX_FLAG_READONLY);
  ASSERT_GE(mailbox_open(box), 0);
  mailbox_free(&box);
}

TEST_F(StorageTest, mail_copy_mail_in_inbox) {
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

  // testdata
  testutils::ItUtils::add_mail(message, mailbox, StorageTest::s_test_mail_user->namespaces);

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
    struct mail_save_context *save_ctx = mailbox_save_alloc(desttrans);  // src save context
    EXPECT_NE(save_ctx, nullptr);

    // 1. read index to get oid
    // 2. move mail to alt
    // 3. update index.
    // see what happends
    std::string alt_dir = "mail_storage_alt_test";
    box->list->set.alt_dir = alt_dir.c_str();
    i_debug("SETTING UPDATE_FLAG");
    mail_update_flags(mail, MODIFY_ADD, (enum mail_flags)MAIL_INDEX_MAIL_FLAG_BACKEND);
    rbox_get_index_record(mail);
    struct rbox_mail *r_mail = (struct rbox_mail *)mail;
    i_debug("end %s", r_mail->rados_mail->get_oid().c_str());
    if (rbox_open_rados_connection(box, true) < 0) {
      FAIL() << "connection error alt";
    } else {
      struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
      // MOVE TO ALT
      std::string oid = r_mail->rados_mail->get_oid();
      librmb::RadosUtils::move_to_alt(oid, mbox->storage->s, mbox->storage->alt, mbox->storage->ms, false);
    }

    int ret2 = mail_get_stream(mail, &hdr_size, &body_size, &input);
    EXPECT_EQ(ret2, 0);
    EXPECT_NE(input, nullptr);
    EXPECT_NE(body_size.physical_size, (uoff_t)0);
    EXPECT_NE(hdr_size.physical_size, (uoff_t)0);

    size_t size = -1;
    int ret_size = i_stream_get_size(input, true, &size);
    EXPECT_EQ(ret_size, 1);
    uoff_t phy_size;
    index_mail_get_physical_size(mail, &phy_size);

    std::string msg3(
        "From: user@domain.org\nDate: Sat, 24 Mar 2017 23:00:00 +0200\nMime-Version: 1.0\nContent-Type: "
        "text/plain; charset=us-ascii\n\nbody\n");

    EXPECT_EQ(phy_size, msg3.length());  // i_stream ads a \r before every \n

    // read the input stream and evaluate content.
    struct const_iovec iov;
    const unsigned char *data = NULL;
    ssize_t ret = 0;
    std::string buff;
    do {
      (void)i_stream_read_data(input, &data, &iov.iov_len, 0);
      if (iov.iov_len == 0) {

    if (input->stream_errno != 0)
      FAIL() << "stream errno";
    break;
      }
      const char *data_t = reinterpret_cast<const char *>(data);
      std::string tmp(data_t, phy_size);
      buff += tmp;
    } while ((size_t)ret == iov.iov_len);

    //    i_debug("data: %s", buff.c_str());
    std::string msg(
        "From: user@domain.org\nDate: Sat, 24 Mar 2017 23:00:00 +0200\nMime-Version: 1.0\nContent-Type: "
        "text/plain; charset=us-ascii\n\nbody\n");

    // validate !
    EXPECT_EQ(buff, msg);

    break;
  }

  if (mailbox_search_deinit(&search_ctx) < 0) {
    FAIL() << "search deinit failed";
  }

  if (mailbox_transaction_commit(&desttrans) < 0) {
    FAIL() << "tnx commit failed";
  }

  if (mailbox_sync(box, static_cast<mailbox_sync_flags>(0)) < 0) {
    FAIL() << "sync failed";
  }

  ASSERT_EQ(1, (int)box->index->map->hdr.messages_count);
  mailbox_free(&box);
}

TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
