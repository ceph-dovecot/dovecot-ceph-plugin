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
/**
 * - add mail via regular alloc, save, commit cycle
 * - change location to alt_storage
 * - copy mail via dovecot calls
 * - validate copy
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

  while (mailbox_search_next(search_ctx, &mail)) {
    save_ctx = mailbox_save_alloc(desttrans);  // src save context

    // 1. read index to get oid
    // 2. move mail to alt
    // 3. update index.
    // see what happends
    std::string alt_dir = "mail_storage_alt_test_copy";
    box->list->set.alt_dir = alt_dir.c_str();

    i_debug("SETTING UPDATE_FLAG ");
    mail_update_flags(mail, MODIFY_ADD, (enum mail_flags)MAIL_INDEX_MAIL_FLAG_BACKEND);
    rbox_get_index_record(mail);
    struct rbox_mail *r_mail = (struct rbox_mail *)mail;
    i_debug("end %s", r_mail->rados_mail->get_oid()->c_str());
    if (rbox_open_rados_connection(box, true) < 0) {
      FAIL() << "connection error alt";
    } else {
      struct rbox_mailbox *mbox = (struct rbox_mailbox *)box;
      // MOVE TO ALT
      std::string oid = *r_mail->rados_mail->get_oid();
      librmb::RadosUtils::move_to_alt(oid, mbox->storage->s, mbox->storage->alt, mbox->storage->ms, false);
    }

    mailbox_save_copy_flags(save_ctx, mail);

    int ret2 = mailbox_copy(&save_ctx, mail);
    EXPECT_EQ(ret2, 0);
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
  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;

  librados::NObjectIterator iter_alt(r_storage->alt->get_io_ctx().nobjects_begin());
  r_storage->ms->get_storage()->set_io_ctx(&r_storage->alt->get_io_ctx());
  std::vector<librmb::RadosMail *> objects_alt;
  while (iter_alt != librados::NObjectIterator::__EndObjectIterator) {
    librmb::RadosMail *obj = new librmb::RadosMail();
    obj->set_oid((*iter_alt).get_oid());
    r_storage->ms->get_storage()->load_metadata(obj);
    objects_alt.push_back(obj);
    iter_alt++;
  }
  r_storage->ms->get_storage()->set_io_ctx(&r_storage->s->get_io_ctx());
  ASSERT_EQ(2, (int)objects_alt.size());
  librmb::RadosMail *mail1 = objects_alt[0];
  librmb::RadosMail *mail2 = objects_alt[1];

  char *val = NULL;
  char *val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " FLAGS " << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }
  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_EXT_REF, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_EXT_REF, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " extref " << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_FROM_ENVELOPE, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_FROM_ENVELOPE, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " from " << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_GUID, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_GUID, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " guid " << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }
  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_MAILBOX_GUID, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_MAILBOX_GUID, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " mguid" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }
  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_ORIG_MAILBOX, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_ORIG_MAILBOX, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " mname" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " psize " << val << " val2 " << val2 << std::endl;
    // ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }
  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_POP3_ORDER, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_POP3_ORDER, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " pop3o" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }
  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_POP3_UIDL, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_POP3_UIDL, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " pop3uidl" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_PVT_FLAGS, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_PVT_FLAGS, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " pvtfl" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " rtime" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_VERSION, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_VERSION, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " versio" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_VIRTUAL_SIZE, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_VIRTUAL_SIZE, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " vsiz" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }
  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_OLDV1_SAVE_TIME, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_OLDV1_SAVE_TIME, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " stim" << std::endl;
    ASSERT_STREQ(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  val = val2 = NULL;
  mail1->get_metadata(librmb::RBOX_METADATA_MAIL_UID, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_MAIL_UID, &val2);
  if (val != NULL && val2 != NULL) {
    std::cout << " mailuid" << std::endl;
    ASSERT_STRNE(val, val2);
  } else if ((val == NULL && val2 != NULL) || (val != NULL && val2 == NULL)) {
    FAIL();
  }

  ASSERT_EQ(2, (int)box->index->map->hdr.messages_count);
  r_storage->alt->delete_mail(mail1);
  r_storage->alt->delete_mail(mail2);

  delete mail1;
  delete mail2;

  mailbox_free(&box);
}

TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
