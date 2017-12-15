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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include "mailbox_tools.h"

namespace librmb {

MailboxTools::MailboxTools(librmb::RadosMailBox* mailbox, std::string base) {
  this->mbox = mailbox;
  this->base_path = base;

  if (base_path.empty()) {
    mailbox_path = this->mbox->get_mailbox_guid();
  } else if (base_path[base_path.length() - 1] == '/') {
    mailbox_path = this->base_path + this->mbox->get_mailbox_guid();
  } else {
    mailbox_path = this->base_path + "/" + this->mbox->get_mailbox_guid();
  }
}

int MailboxTools::init_mailbox_dir() {
  struct stat st = {0};

  if (stat(this->base_path.c_str(), &st) == -1) {
    if (mkdir(this->base_path.c_str(), 0700) < 0) {
      return -1;
    }
  }

  st = {0};
  if (stat(mailbox_path.c_str(), &st) == -1) {
    if (mkdir(mailbox_path.c_str(), 0700) < 0) {
      return -1;
    }
  }
  return 0;
}

int MailboxTools::delete_mail(librmb::RadosMailObject* mail_obj) {
  if (mail_obj == nullptr) {
    return -1;
  }
  std::string filename;
  if (build_filename(mail_obj, &filename) < 0) {
    return -1;
  }
  std::string file_path = mailbox_path + "/" + filename;
  return unlink(file_path.c_str());
}

int MailboxTools::delete_mailbox_dir() {
  if (this->mailbox_path.empty() || this->base_path.empty()) {
    return -1;
  }
  if (rmdir(this->mailbox_path.c_str()) < 0) {
    return -1;
  }
  if (rmdir(this->base_path.c_str()) < 0) {
    return -1;
  }
  return 0;
}

int MailboxTools::save_mail(librmb::RadosMailObject* mail_obj) {
  if (mail_obj == nullptr) {
    return -1;
  }
  std::string filename;
  if (build_filename(mail_obj, &filename) < 0) {
    return -1;
  }

  std::string file_path = mailbox_path + "/" + filename;
  std::cout << " writing mail to " << file_path << std::endl;
  std::ofstream myfile(file_path, std::ofstream::binary | std::ofstream::out);
  if (!myfile.is_open()) {
    return -1;
  }
  myfile.write(mail_obj->get_mail_buffer()->to_str().c_str(), mail_obj->get_mail_size());
  myfile.close();
  return 0;
}

int MailboxTools::build_filename(librmb::RadosMailObject* mail_obj, std::string* filename) {
  if (mail_obj == nullptr || !filename->empty()) {
    return -1;
  }

  std::stringstream ss;
  ss << mail_obj->get_metadata(librmb::RBOX_METADATA_MAIL_UID) << ".";
  ss << mail_obj->get_oid();
  *filename = ss.str();
  return filename->empty() ? -1 : 0;
}

}  // namespace librmb
