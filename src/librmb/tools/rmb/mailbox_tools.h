/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_
#define SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_

#include "rados-mail-object.h"
#include "rmb.h"
#include <string>

class MailboxTools {
 public:
  MailboxTools(librmb::RadosMailBox* mailbox, std::string base) {
    this->mbox = mailbox;
    this->base_path = base;
    mailbox_path = this->base_path + "/" + this->mbox->get_mailbox_guid();
  }
  ~MailboxTools() {}

  void init_mailbox_dir();
  void save_mail(librmb::RadosMailObject* mail_obj);
  void delete_mailbox_dir();
  int delete_mail(librmb::RadosMailObject* mail_obj);

  void build_filename(librmb::RadosMailObject* mail_obj, std::string& filename);

 private:
  librmb::RadosMailBox* mbox;
  std::string base_path;
  std::string mailbox_path;
};

#endif /* SRC_LIBRMB_TOOLS_RMB_MAILBOX_TOOLS_H_ */
