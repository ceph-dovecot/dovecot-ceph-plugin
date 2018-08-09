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
#include "rbox-copy.h"

using ::testing::AtLeast;
using ::testing::Return;

#pragma GCC diagnostic pop

#define DEFAULT_ENVELOPE_SENDER "MAILER-DAEMON"

/* After buffer grows larger than this, create a temporary file to /tmp
   where to read the mail. */
#define MAIL_MAX_MEMORY_BUFFER (1024 * 128)

static int test_rbox_mail_copy(struct mail_save_context *_ctx, struct mail *mail) {
  _ctx->saving = false;
  mail->box->storage->name = "mdbox";
  return rbox_mail_copy(_ctx, mail);
}

TEST_F(StorageTest, init) {}

TEST_F(StorageTest, mail_doveadm_backup_copy_mail_in_inbox) {
  struct mailbox_transaction_context *desttrans;
  struct mail_save_context *save_ctx;
  struct mail *mail;
  struct mail_search_context *search_ctx;
  struct mail_search_args *search_args;
  struct mail_search_arg *sarg;

  const char *message =
      "From: user@domain.org\n"
      "Date: Mon, 06 Nov 2017 09:20:00 +0100\n"
      "Mime-Version: 1.0\n"
      "Content-Type: text/plain; charset=us-ascii\n"
      "\n"
      "Hello Test!\n";

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

  box->v.copy = test_rbox_mail_copy;

  search_ctx = mailbox_search_init(desttrans, search_args, NULL, static_cast<mail_fetch_field>(0), NULL);
  mail_search_args_unref(&search_args);

  while (mailbox_search_next(search_ctx, &mail)) {
    save_ctx = mailbox_save_alloc(desttrans);  // src save context
    mailbox_save_copy_flags(save_ctx, mail);
    int ret2 = mailbox_copy(&save_ctx, mail);
    EXPECT_EQ(ret2, 0);
    break;  // only deliver one mail.
  }

  if (mailbox_search_deinit(&search_ctx) < 0) {
    FAIL() << "search deinit failed";
  }
  if (mailbox_transaction_commit(&desttrans) < 0) {
    FAIL() << "transaction commit failed";
  }
  if (mailbox_sync(box, static_cast<mailbox_sync_flags>(0)) < 0) {
    FAIL() << "sync failed";
  }

  struct rbox_storage *r_storage = (struct rbox_storage *)box->storage;
  librados::NObjectIterator iter(r_storage->s->get_io_ctx().nobjects_begin());
  std::vector<librmb::RadosMailObject *> objects;
  while (iter != r_storage->s->get_io_ctx().nobjects_end()) {
    librmb::RadosMailObject *obj = new librmb::RadosMailObject();
    obj->set_oid((*iter).get_oid());
    r_storage->ms->get_storage()->load_metadata(obj);
    objects.push_back(obj);
    iter++;
  }

  // compare objects
  ASSERT_EQ(2, (int)objects.size());

  librmb::RadosMailObject *mail1 = objects[0];
  librmb::RadosMailObject *mail2 = objects[1];

  std::string val;
  std::string val2;
  mail1->get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_OLDV1_FLAGS, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_EXT_REF, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_EXT_REF, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_FROM_ENVELOPE, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_FROM_ENVELOPE, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_GUID, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_GUID, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_MAILBOX_GUID, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_MAILBOX_GUID, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_ORIG_MAILBOX, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_ORIG_MAILBOX, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_PHYSICAL_SIZE, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_POP3_ORDER, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_POP3_ORDER, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_POP3_UIDL, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_POP3_UIDL, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_PVT_FLAGS, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_PVT_FLAGS, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_RECEIVED_TIME, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_VERSION, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_VERSION, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_VIRTUAL_SIZE, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_VIRTUAL_SIZE, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_OLDV1_SAVE_TIME, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_OLDV1_SAVE_TIME, &val2);
  ASSERT_EQ(val, val2);

  mail1->get_metadata(librmb::RBOX_METADATA_MAIL_UID, &val);
  mail2->get_metadata(librmb::RBOX_METADATA_MAIL_UID, &val2);
  ASSERT_NE(val, val2);
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
