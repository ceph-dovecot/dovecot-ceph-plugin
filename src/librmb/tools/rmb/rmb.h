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
#include <map>

namespace librmb {

class RadosMailBox {
 public:
  RadosMailBox(std::string mailbox_guid, int mail_count) {
    this->mail_count = mail_count;
    this->mailbox_guid = mailbox_guid;
    this->mailbox_size = 0;
    total_mails = 0;
  }
  virtual ~RadosMailBox() {}

  void add_mail(RadosMailObject *mail) {
    total_mails++;
    if (xattr_filter.size() == 0) {
      mails.push_back(mail);
      return;
    }
    for (std::map<std::string, std::string>::iterator it = xattr_filter.begin(); it != xattr_filter.end(); ++it) {
      if (mail->get_xattr()->find(it->first) != mail->get_xattr()->end()) {
      /*  std::cout << "comparing: " << it->second
                  << " with : " << mail->get_xvalue(it->first).substr(0, mail->get_xvalue(it->first).length() - 1)
                  << " org: " << mail->get_xvalue(it->first) << std::endl;*/
        if (it->second.compare(mail->get_xvalue(it->first).substr(0, mail->get_xvalue(it->first).length() - 1)) == 0) {
          mails.push_back(mail);
        }

        return;
      }
    }
  }

  inline std::string to_string() {
    std::ostringstream ss;
    ss << std::endl
       << "MAILBOX: " << (char)RBOX_METADATA_MAILBOX_GUID << "(mailbox_guid)=" << this->mailbox_guid << std::endl
       << "         mail_total=" << total_mails << ", mails_displayed=" << mails.size() << std::endl
       << "         mailbox_size=" << mailbox_size << " bytes " << std::endl;

    std::string padding("         ");
    for (std::vector<RadosMailObject *>::iterator it = mails.begin(); it != mails.end(); ++it) {
      ss << (*it)->to_string(padding);
    }
    return ss.str();
  }
  inline void add_to_mailbox_size(const uint64_t &mail_size) { this->mailbox_size += mail_size; }
  void set_mails(std::vector<RadosMailObject *> mails) { this->mails = mails; }

  std::map<std::string, std::string> &get_xattr_filter() { return this->xattr_filter; }
  void set_xattr_filter(std::map<std::string, std::string> &filter) { this->xattr_filter = filter; }

 private:
  std::map<std::string, std::string> xattr_filter;

  std::string mailbox_guid;
  int mail_count;
  uint64_t mailbox_size;
  std::vector<RadosMailObject *> mails;
  uint64_t total_mails;
};
}  // namespace librmb

#endif /* SRC_LIBRMB_TOOLS_RBOX_LIST_OBJECTS_H_ */
