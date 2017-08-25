/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "gtest/gtest.h"
#include "rados-storage.h"
#include "rados-cluster.h"
#include "rados-mail-object.h"
#include <rados/librados.hpp>
#include "../../librmb/tools/rmb/ls_cmd_parser.h"
#include "../../librmb/tools/rmb/mailbox_tools.h"

TEST(rmb, test_cmd_parser) {
  std::string key = "M";
  std::string key2 = "U";
  std::string key3 = "R";
  //                             917378644
  std::string ls = "M=abc;U=1;R<2013-12-04 15:03";
  librmb::CmdLineParser parser(ls);
  EXPECT_TRUE(parser.parse_ls_string());

  EXPECT_EQ(3, parser.get_predicates().size());

  EXPECT_TRUE(parser.contains_key(key));
  EXPECT_TRUE(parser.contains_key(key2));
  EXPECT_TRUE(parser.contains_key(key3));

  librmb::rbox_metadata_key k = static_cast<librmb::rbox_metadata_key>('M');
  EXPECT_EQ(k, librmb::RBOX_METADATA_MAILBOX_GUID);
  librmb::Predicate *p = parser.get_predicate(key);
  std::string value = "abc";
  EXPECT_TRUE(p->eval(value));

  librmb::Predicate *p2 = parser.get_predicate(key2);
  value = "1";
  EXPECT_TRUE(p2->eval(value));

  librmb::Predicate *p3 = parser.get_predicate(key3);

  value = "1503393219";
  EXPECT_FALSE(p3->eval(value));

  // value = "1086165760";
  // EXPECT_TRUE(p3->eval(value));

  delete p;
  delete p2;
  delete p3;
}

TEST(rmb1, date_arg) {
  librmb::Predicate *p = new librmb::Predicate();

  std::string date = "2013-12-04 15:03";
  time_t t;
  p->convert_str_to_time_t(date, &t);
  std::cout << "time t " << t << std::endl;
  EXPECT_GT(t, -1);
  time_t t2 = 1503393219;
  std::string val;
  p->convert_time_t_to_str(t, val);
  std::cout << val << std::endl;
  p->convert_time_t_to_str(t2, val);
  std::cout << val << std::endl;

  time_t t3 = 1086165760;
  p->convert_time_t_to_str(t3, val);
  std::cout << val << std::endl;

  delete p;
}

TEST(rmb1, save_mail) {
  std::string mbox_guid = "abc";
  librmb::RadosMailBox mbox(mbox_guid, 1, mbox_guid);

  std::string base_path = "test";
  MailboxTools tools(&mbox, base_path);

  int init = tools.init_mailbox_dir();
  EXPECT_EQ(0, init);

  librmb::RadosMailObject mail;
  librados::bufferlist bl;
  bl.append("1");
  (*mail.get_xattr())["U"] = bl;
  std::string mail_guid = "defg";
  std::string mail_content = "hallo welt\nbababababa\n";
  mail.set_oid(mail_guid);
  mail.set_mail_buffer(&mail_content[0u]);
  uint64_t size = mail_content.length();
  mail.set_object_size(size);
  int save = tools.save_mail(&mail);
  EXPECT_EQ(0, save);

  int ret = tools.delete_mail(&mail);
  int ret_rm_dir = tools.delete_mailbox_dir();
  EXPECT_EQ(0, ret);
  EXPECT_EQ(0, ret_rm_dir);
}

TEST(rmb1, path_tests) {
  std::string mbox_guid = "abc";
  librmb::RadosMailBox mbox(mbox_guid, 1, mbox_guid);

  std::string base_path = "test";
  MailboxTools tools(&mbox, base_path);
  EXPECT_EQ("test/abc", tools.get_mailbox_path());
  std::string test_path = "test/";
  MailboxTools tools2(&mbox, test_path);
  EXPECT_EQ("test/abc", tools2.get_mailbox_path());

  std::string test_path2 = "";
  MailboxTools tools3(&mbox, test_path2);
  EXPECT_EQ("abc", tools3.get_mailbox_path());
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
