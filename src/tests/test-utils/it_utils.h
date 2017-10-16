/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_TESTS_TEST_UTILS_IT_UTILS_H_
#define SRC_TESTS_TEST_UTILS_IT_UTILS_H_

extern "C" {
#include "lib.h"
#include "mail-user.h"
#include "mail-storage.h"
#include "mail-storage-service.h"
#include "mail-namespace.h"
#include "mailbox-list.h"
#include "ioloop.h"
#include "istream.h"
#include "doveadm.h"
#include "doveadm-util.h"
#include "doveadm-cmd.h"
#include "doveadm-mailbox-list-iter.h"
#include "doveadm-mail-iter.h"
#include "doveadm-mail.h"
#include "mail-search-build.h"

#include "libdict-rados-plugin.h"
#include "mail-search-parser-private.h"
#include "mail-search.h"
}
#include "rbox-storage.hpp"

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

class ItUtils {
 public:
  ItUtils();
  virtual ~ItUtils();
  static void add_mail(const char *message, const char *mailbox, struct mail_namespace *_ns);
  static void add_mail(const char *message, const char *mailbox, struct mail_namespace *_ns,
                       librmb::RadosStorage *storage_impl);
  static void add_mail(struct mail_save_context *save_ctx, struct istream *input, struct mailbox *box,
                       struct mailbox_transaction_context *trans);
};

} /* namespace testutils */

#endif /* SRC_TESTS_TEST_UTILS_IT_UTILS_H_ */
