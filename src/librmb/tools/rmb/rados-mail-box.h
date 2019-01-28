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

#ifndef SRC_LIBRMB_TOOLS_RMB_RMB_H_
#define SRC_LIBRMB_TOOLS_RMB_RMB_H_

#include <string>
#include <sstream>
#include <vector>
#include <map>

#include "../../rados-mail.h"
#include "ls_cmd_parser.h"
#include "rados-util.h"
namespace librmb {

class RadosMailBox {
 public:
  RadosMailBox(const std::string &_mailbox_guid, int _mail_count, const std::string &_mbox_orig_name)
      : mailbox_guid(_mailbox_guid), mail_count(_mail_count), mbox_orig_name(_mbox_orig_name) {
    this->mailbox_size = 0;
    this->total_mails = 0;
    this->parser = nullptr;
  }
  virtual ~RadosMailBox() {}

  void add_mail(RadosMail *mail) {
    total_mails++;
    if (!mail->is_valid()) {
      mails.push_back(mail);
      return;
    }
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
      if (mail->get_metadata()->find(it->first) != mail->get_metadata()->end()) {
        std::string key = it->first;
        char *value;
        RadosUtils::get_metadata(key, mail->get_metadata(), &value);
        if (it->second->eval(value)) {
          mails.push_back(mail);
        }
        return;
      } else if (it->first.compare("-") == 0) {
        mails.push_back(mail);
      }
    }
  }

  inline std::string to_string() {
    std::ostringstream ss;
    ss << std::endl
       << "MAILBOX: " << static_cast<char>(RBOX_METADATA_MAILBOX_GUID) << "(mailbox_guid)=" << this->mailbox_guid
       << std::endl
       << "         " << static_cast<char>(RBOX_METADATA_ORIG_MAILBOX) << "(mailbox_orig_name)=" << mbox_orig_name
       << std::endl

       << "         mail_total=" << total_mails << ", mails_displayed=" << mails.size() << std::endl
       << "         mailbox_size=" << mailbox_size << " bytes " << std::endl;

    std::string padding("         ");
    for (std::vector<RadosMail *>::iterator it = mails.begin(); it != mails.end(); ++it) {
      ss << (*it)->to_string(padding);
    }
    return ss.str();
  }
  inline void add_to_mailbox_size(const uint64_t &_mailbox_size) { this->mailbox_size += _mailbox_size; }
  void set_mails(const std::vector<RadosMail *> &_mails) { this->mails = _mails; }

  CmdLineParser *get_xattr_filter() { return this->parser; }
  void set_xattr_filter(CmdLineParser *_parser) { this->parser = _parser; }
  std::vector<RadosMail *> &get_mails() { return this->mails; }

  std::string &get_mailbox_guid() { return this->mailbox_guid; }
  void set_mailbox_guid(const std::string &_mailbox_guid) { this->mailbox_guid = _mailbox_guid; }
  void set_mailbox_orig_name(const std::string &_mbox_orig_name) { this->mbox_orig_name = _mbox_orig_name; }
  int &get_mail_count() { return this->mail_count; }

 private:
  CmdLineParser *parser;

  std::string mailbox_guid;
  int mail_count;
  uint64_t mailbox_size;
  std::vector<RadosMail *> mails;
  uint64_t total_mails;
  std::string mbox_orig_name;
};
}  // namespace librmb

#endif  // SRC_LIBRMB_TOOLS_RMB_RMB_H_
