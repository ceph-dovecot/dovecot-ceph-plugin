/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_
#define SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_

#include <string>
#include "rados-mail-object.h"
#include "rmb.h"

namespace librmb {
class MailboxTools {
 public:
  MailboxTools(librmb::RadosMailBox* mailbox, std::string base);
  ~MailboxTools() {}

  int init_mailbox_dir();
  int save_mail(librmb::RadosMailObject* mail_obj);
  int delete_mailbox_dir();
  int delete_mail(librmb::RadosMailObject* mail_obj);

  int build_filename(librmb::RadosMailObject* mail_obj, /*const*/ std::string& filename);

  std::string& get_mailbox_path() { return this->mailbox_path; }

 private:
  librmb::RadosMailBox* mbox;
  std::string base_path;
  std::string mailbox_path;
};
};  // namespace librmb

#endif  // SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_
