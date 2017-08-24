/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */


#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fstream>
#include <iostream>

#include "mailbox_tools.h"

void MailboxTools::init_mailbox_dir() {
  struct stat st = {0};

  if (stat(this->base_path.c_str(), &st) == -1) {
    mkdir(this->base_path.c_str(), 0700);
  }

  st = {0};
  if (stat(mailbox_path.c_str(), &st) == -1) {
    mkdir(mailbox_path.c_str(), 0700);
  }
}
int MailboxTools::delete_mail(librmb::RadosMailObject* mail_obj) {
  std::string filename;

  build_filename(mail_obj, filename);
  std::string file_path = mailbox_path + "/" + filename;
  return unlink(file_path.c_str());
}
void MailboxTools::delete_mailbox_dir() {
  rmdir(this->mailbox_path.c_str());
  rmdir(this->base_path.c_str());
}
void MailboxTools::save_mail(librmb::RadosMailObject* mail_obj) {
  std::string filename;
  build_filename(mail_obj, filename);
  std::string file_path = mailbox_path + "/" + filename;
  std::cout << " writing mail to " << file_path << std::endl;
  std::ofstream myfile;
  myfile.open(file_path);
  myfile << mail_obj->get_mail_buffer();
  myfile.close();
  std::ofstream ofs;
}

void MailboxTools::build_filename(librmb::RadosMailObject* mail_obj, std::string& filename) {
  std::stringstream ss;
  ss << mail_obj->get_xvalue(librmb::RBOX_METADATA_MAIL_UID) << ".";
  ss << mail_obj->get_oid();
  filename = ss.str();
}
