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
#include "time.h"
#include "guid.h"
}
#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"
#include "dovecot-ceph-plugin-config.h"
#include "../test-utils/it_utils.h"
#include "rbox-mail.h"
using ::testing::AtLeast;
using ::testing::Return;

TEST_F(StorageTest, init) {}

TEST_F(StorageTest, mailbox_open_inbox) {
  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", MAILBOX_FLAG_READONLY);
  ASSERT_GE(mailbox_open(box), 0);
  mailbox_free(&box);
}
/**
 * - add mail via regular alloc, save, commit cycle
 * - copy mail via dovecot calls
 * - validate copy
 *
 */
TEST_F(StorageTest, check_metadata) {
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

  while (mailbox_search_next(search_ctx, &mail)) {
    save_ctx = mailbox_save_alloc(desttrans);  // src save context

    const char *value = NULL;
    if (mail_get_special(mail, MAIL_FETCH_GUID, &value) < 0) {
      FAIL();
    }

    struct rbox_mail *rmail = (struct rbox_mail *)mail;
    ASSERT_FALSE(guid_128_is_empty(rmail->index_guid));
    const char *guid = guid_128_to_string(rmail->index_guid);

    // test re-write guid to index. simulate index_guid is empty.
    guid_128_empty(rmail->index_guid);
    char *value_2 = NULL;
    if (mail_get_special(mail, MAIL_FETCH_GUID, &value_2) < 0) {
      FAIL();
    }
    // we need to free it here!!!
    i_free(value_2);

    ASSERT_FALSE(guid_128_is_empty(rmail->index_guid));
    char *guid2 = guid_128_to_string(rmail->index_guid);

    i_debug("GUID values: %s : metadata in mails_cache: rmail->index_guid '%s'", value, guid);
    ASSERT_STREQ(guid, guid2);
    ASSERT_STREQ(value, guid);

    // check that index is still ok !
    const void *rec_data = NULL;
    struct rbox_mailbox *rbox = (struct rbox_mailbox *)mail->transaction->box;
    mail_index_lookup_ext(mail->transaction->view, mail->seq, rbox->ext_id, &rec_data, NULL);
    if (rec_data == NULL) {
      FAIL();
    }
    const struct obox_mail_index_record *obox_rec = static_cast<const struct obox_mail_index_record *>(rec_data);
    guid_128_t obox_guid;
    guid_128_t obox_oid;

    memcpy(obox_guid, obox_rec->guid, sizeof(obox_rec->guid));
    memcpy(obox_oid, obox_rec->oid, sizeof(obox_rec->oid));
    ASSERT_STREQ(guid_128_to_string(obox_guid), guid2);
    ASSERT_STREQ(guid_128_to_string(obox_oid), guid_128_to_string(rmail->index_oid));

    const char *value2 = NULL;
    if (mail_get_special(mail, MAIL_FETCH_MAILBOX_NAME, &value2) < 0) {
      FAIL();
    }
    ASSERT_TRUE(value2 != NULL);

    time_t date_r = -1;
    if (mail_get_save_date(mail, &date_r) < 0) {
      FAIL();
    }

    i_debug("save date value %ld", date_r);
    char buff[20];
    strftime(buff, 20, "%Y-%m-%d %H:%M:%S", localtime(&date_r));
    i_debug("save date value %ld = %s", date_r, buff);
    time_t date_recv = -1;
    if (mail_get_received_date(mail, &date_recv) < 0) {
      FAIL();
    }

    i_debug("recv date value %ld", date_recv);
    char buff2[20];
    strftime(buff2, 20, "%Y-%m-%d %H:%M:%S", localtime(&date_recv));
    i_debug("recv date value %ld = %s", date_recv, buff2);

    char *val3 = NULL;
    rmail->rados_mail->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, &val3);
    ASSERT_TRUE(val3 != NULL);

    ASSERT_EQ(rmail->rados_mail->get_metadata()->size(), 8);
    // load from index.
    time_t date_recv2 = -1;
    if (mail_get_received_date(mail, &date_recv2) < 0) {
      FAIL();
    }
    i_debug("here is the recv2 date value %ld", date_recv2);

    ASSERT_EQ(date_recv, date_recv2);

    uoff_t size_r;
    if (mail_get_physical_size(mail, &size_r) < 0) {
      FAIL();
    }
    i_debug("physical size %ld", size_r);

    uoff_t size_v;
    if (mail_get_virtual_size(mail, &size_v) < 0) {
      FAIL();
    }
    i_debug("virtual size %ld", size_v);

    const char *value3 = NULL;
    if (mail_get_special(mail, MAIL_FETCH_MAILBOX_NAME, &value3) < 0) {
      FAIL();
    }
    ASSERT_TRUE(value3 != NULL);
    ASSERT_EQ(rmail->rados_mail->get_metadata()->size(), 8);
    break;  // only move one mail.
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

  mailbox_free(&box);
}

TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
