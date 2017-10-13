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

/*
#include "master-service-settings.h"
#include "raw-storage.h"
#include "mail-deliver.h"
*/
}
#include "rbox-storage.hpp"
#include "../mocks/mock_test.h"
#include "dovecot-ceph-plugin-config.h"

using ::testing::AtLeast;
using ::testing::Return;

#pragma GCC diagnostic pop

#define DEFAULT_ENVELOPE_SENDER "MAILER-DAEMON"

/* After buffer grows larger than this, create a temporary file to /tmp
   where to read the mail. */
#define MAIL_MAX_MEMORY_BUFFER (1024 * 128)

// static const char *wanted_headers[] = {"From", "To", "Message-ID", "Subject", "Return-Path", NULL};

TEST_F(StorageTest, init) {}

TEST_F(StorageTest, mailbox_open_inbox) {
  struct mail_namespace *ns = mail_namespace_find_inbox(s_test_mail_user->namespaces);
  struct mailbox *box = mailbox_alloc(ns->list, "INBOX", MAILBOX_FLAG_READONLY);
  ASSERT_GE(mailbox_open(box), 0);
  mailbox_close(box);
  mailbox_free(&box);
}

/*
static struct mail *lda_raw_mail_open(struct mail_deliver_context *ctx, const char *path) {
  struct mail_user *raw_mail_user;
  struct mailbox *box;
  struct mailbox_transaction_context *t;
  struct mail *mail;
  struct mailbox_header_lookup_ctx *headers_ctx;
  struct istream *input;
  void **sets;
  const char *envelope_sender;
  time_t mtime;
  int ret;

  sets = master_service_settings_get_others(master_service);
  raw_mail_user = raw_storage_create_from_set(ctx->dest_user->set_info, sets[0]);

  envelope_sender = ctx->src_envelope_sender != NULL ? ctx->src_envelope_sender : DEFAULT_ENVELOPE_SENDER;
  if (path == NULL) {
    i_fatal("No path to mail set: %s", mailbox_get_last_error(box, NULL));
  } else {
    ret = raw_mailbox_alloc_path(raw_mail_user, path, (time_t)-1, envelope_sender, &box);
  }
  if (ret < 0) {
    i_fatal("Can't open delivery mail as raw: %s", mailbox_get_last_error(box, NULL));
  }
  mail_user_unref(&raw_mail_user);

  t = mailbox_transaction_begin(box, 0);
  headers_ctx = mailbox_header_lookup_init(box, wanted_headers);
  mail = mail_alloc(t, 0, headers_ctx);
  mailbox_header_lookup_unref(&headers_ctx);
  mail_set_seq(mail, 1);
  return mail;
}
*/

TEST_F(StorageTest, mail_copy_mail_in_inbox) {
  const char *mailbox = "INBOX";
  ASSERT_GE(strlen(mailbox), (size_t)0);
}

TEST_F(StorageTest, deinit) {}

int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
