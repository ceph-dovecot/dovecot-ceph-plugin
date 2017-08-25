/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_LIBRMB_TOOLS_RBOX_LIST_OBJECTS_H_
#define SRC_LIBRMB_TOOLS_RBOX_LIST_OBJECTS_H_

#include <string>
#include <sstream>
#include "rados-mail-object.h"
#include <vector>
#include <map>
#include "ls_cmd_parser.h"
namespace librmb {

class RadosMailBox {
 public:
  RadosMailBox(std::string _mailbox_guid, int _mail_count, std::string _mbox_orig_name) {
    this->mail_count = _mail_count;
    this->mailbox_guid = _mailbox_guid;
    this->mailbox_size = 0;
    this->total_mails = 0;
    this->parser = nullptr;
    this->mbox_orig_name = _mbox_orig_name;
  }
  virtual ~RadosMailBox() {}

  void add_mail(RadosMailObject *mail) {
    total_mails++;
    if (parser == nullptr) {
      mails.push_back(mail);
      return;
    }
    if (parser->get_predicates().size() == 0) {
      mails.push_back(mail);
      return;
    }
    for (std::map<std::string, Predicate *>::iterator it = parser->get_predicates().begin();
         it != parser->get_predicates().end(); ++it) {
      // std::cout << " looking for: " << it->first << std::endl;
      if (mail->get_xattr()->find(it->first) != mail->get_xattr()->end()) {
        /*  std::cout << "comparing: " << it->second
                    << " with : " << mail->get_xvalue(it->first).substr(0, mail->get_xvalue(it->first).length() - 1)
                    << " org: " << mail->get_xvalue(it->first) << std::endl;*/
        //   std::cout << " found : " << it->first << std::endl;

        std::string key = it->first;
        if (it->second->eval(mail->get_xvalue(key))) {
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
       << "         " << (char)RBOX_METADATA_ORIG_MAILBOX << "(mailbox_orig_name)=" << mbox_orig_name << std::endl

       << "         mail_total=" << total_mails << ", mails_displayed=" << mails.size() << std::endl
       << "         mailbox_size=" << mailbox_size << " bytes " << std::endl;

    std::string padding("         ");
    for (std::vector<RadosMailObject *>::iterator it = mails.begin(); it != mails.end(); ++it) {
      ss << (*it)->to_string(padding);
    }
    return ss.str();
  }
  inline void add_to_mailbox_size(const uint64_t &_mailbox_size) { this->mailbox_size += _mailbox_size; }
  void set_mails(std::vector<RadosMailObject *> _mails) { this->mails = _mails; }

  CmdLineParser *get_xattr_filter() { return this->parser; }
  void set_xattr_filter(CmdLineParser *_parser) { this->parser = _parser; }
  std::vector<RadosMailObject *> &get_mails() { return this->mails; }

  std::string &get_mailbox_guid() { return this->mailbox_guid; }
  void set_mailbox_guid(std::string &_mailbox_guid) { this->mailbox_guid = _mailbox_guid; }
  void set_mailbox_orig_name(std::string &_mbox_orig_name) { this->mbox_orig_name = _mbox_orig_name; }
  int &get_mail_count() { return this->mail_count; }

 private:
  CmdLineParser *parser;

  std::string mailbox_guid;
  int mail_count;
  uint64_t mailbox_size;
  std::vector<RadosMailObject *> mails;
  uint64_t total_mails;
  std::string mbox_orig_name;
};
}  // namespace librmb

#endif /* SRC_LIBRMB_TOOLS_RBOX_LIST_OBJECTS_H_ */
