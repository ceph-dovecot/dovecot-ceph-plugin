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

#ifndef SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_
#define SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_

#include <string>
#include "rados-mail-object.h"
#include "rados-mail-box.h"

namespace librmb {
class MailboxTools {
 public:
  MailboxTools(librmb::RadosMailBox* mailbox, std::string base);
  ~MailboxTools() {}

  int init_mailbox_dir();
  int save_mail(librmb::RadosMailObject* mail_obj);
  int delete_mailbox_dir();
  int delete_mail(librmb::RadosMailObject* mail_obj);

  int build_filename(librmb::RadosMailObject* mail_obj, std::string* filename);

  std::string& get_mailbox_path() { return this->mailbox_path; }

 private:
  librmb::RadosMailBox* mbox;
  std::string base_path;
  std::string mailbox_path;
};
};  // namespace librmb

#endif  // SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_
