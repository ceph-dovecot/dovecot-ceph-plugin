/*
 * rbox_list_objects.h
 *
 *  Created on: Aug 21, 2017
 *      Author: jan
 */

#ifndef SRC_LIBRMB_TOOLS_RBOX_LIST_OBJECTS_H_
#define SRC_LIBRMB_TOOLS_RBOX_LIST_OBJECTS_H_

#include <string>
#include <sstream>
#include "rados-mail-object.h"
#include <vector>

namespace librmb {

class RadosMailBox {
 public:
  RadosMailBox(std::string mailbox_guid, int mail_count) {
    this->mail_count = mail_count;
    this->mailbox_guid = mailbox_guid;
    this->mailbox_size = 0;
  }
  virtual ~RadosMailBox() {}

  void add_mail(RadosMailObject *mail) { mails.push_back(mail); }

  inline std::string to_string() {
    std::ostringstream ss;
    ss << std::endl
       << "MAILBOX: " << (char)RBOX_METADATA_MAILBOX_GUID << "(mailbox_guid)=" << this->mailbox_guid << std::endl
       << "         mail_count=" << mails.size() << std::endl
       << "         mailbox_size=" << mailbox_size << " bytes " << std::endl;

    std::string padding("         ");
    for (std::vector<RadosMailObject *>::iterator it = mails.begin(); it != mails.end(); ++it) {
      ss << (*it)->to_string(padding);
    }
    return ss.str();
  }
  inline void add_to_mailbox_size(const uint64_t &mail_size) { this->mailbox_size += mail_size; }
  void set_mails(std::vector<RadosMailObject *> mails) { this->mails = mails; }

 private:
  std::string mailbox_guid;
  int mail_count;
  uint64_t mailbox_size;
  std::vector<RadosMailObject *> mails;
};
}  // namespace librmb

#endif /* SRC_LIBRMB_TOOLS_RBOX_LIST_OBJECTS_H_ */
