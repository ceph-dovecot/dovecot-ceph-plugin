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
/**
 * - add mail via regular alloc, save, commit cycle
 * - copy mail via dovecot calls
 * - validate copy
 *
 */
TEST_F(StorageTest, mail_copy_mail_in_inbox) {
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

  i_debug("mail_ added ");

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
  int ret2 = 0;
  while (mailbox_search_next(search_ctx, &mail)) {
    save_ctx = mailbox_save_alloc(desttrans);  // src save context
    mailbox_save_copy_flags(save_ctx, mail);
    ret2 = mailbox_copy(&save_ctx, mail);
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

  EXPECT_EQ(ret2, 0);
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
  librados::NObjectIterator iter(r_storage->s->get_io_ctx().nobjects_begin());
  std::vector<librmb::RadosMail *> objects;
  while (iter != librados::NObjectIterator::__EndObjectIterator) {
    librmb::RadosMail *obj = new librmb::RadosMail();
    obj->set_oid((*iter).get_oid());
    r_storage->ms->get_storage()->load_metadata(obj);
    objects.push_back(obj);
    i_info("found mail....");
    iter++;
  }
  // i_debug("load metadat finished");
  // compare objects
  ASSERT_EQ(2, (int)objects.size());

  librmb::RadosMail *mail1 = objects[0];
  librmb::RadosMail *mail2 = objects[1];

  char *val = NULL;
  char *val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);

  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_EXT_REF, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_EXT_REF, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);

  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_FROM_ENVELOPE, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_FROM_ENVELOPE, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);

  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_GUID, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_GUID, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_MAILBOX_GUID, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_MAILBOX_GUID, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_ORIG_MAILBOX, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_ORIG_MAILBOX, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);

  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_POP3_ORDER, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_POP3_ORDER, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_POP3_UIDL, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_POP3_UIDL, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_PVT_FLAGS, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_PVT_FLAGS, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_VERSION, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_VERSION, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_VIRTUAL_SIZE, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_VIRTUAL_SIZE, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_OLDV1_SAVE_TIME, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_OLDV1_SAVE_TIME, mail2->get_metadata(), &val2);
  ASSERT_STREQ(val, val2);
  val = val2 = NULL;
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_MAIL_UID, mail1->get_metadata(), &val);
  librmb::RadosUtils::get_metadata(librmb::RBOX_METADATA_MAIL_UID, mail2->get_metadata(), &val2);
  ASSERT_STRNE(val, val2);

  ASSERT_EQ(2, (int)box->index->map->hdr.messages_count);
  delete mail1;
  delete mail2;
  mailbox_free(&box);
}

TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
