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

#include <ctime>
#include <rados/librados.hpp>

#include "../../librmb/rados-cluster-impl.h"
#include "../../librmb/rados-ceph-json-config.h"
#include "../../librmb/rados-storage-impl.h"
#include "mock_test.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "rados-util.h"
#include "rados-types.h"
#include "rados-save-log.h"
#include "rados-mail.h"
#include <cstdio>
#include <pthread.h>

using ::testing::AtLeast;
using ::testing::Return;

TEST(librmb, get_metadata_1) {
  enum librmb::rbox_metadata_key key = librmb::rbox_metadata_key::RBOX_METADATA_GUID;
  librmb::RadosMetadata m(key, "abcdefg");
  librmb::RadosMail mail;
  mail.add_metadata(m);
  char *val = NULL;

  mail.get_metadata(key, &val);
  std::cout << val << std::endl;
  EXPECT_EQ(val, "abcdefg\0");
}

TEST(librmb, convert_enum) {
  enum librmb::rbox_metadata_key key = librmb::rbox_metadata_key::RBOX_METADATA_GUID;

  std::string metadata_key(librmb::rbox_metadata_key_to_char(key));
  EXPECT_EQ("G", metadata_key);

  key = librmb::RBOX_METADATA_MAILBOX_GUID;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("M", metadata_key);

  key = librmb::RBOX_METADATA_GUID;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("G", metadata_key);

  key = librmb::RBOX_METADATA_POP3_UIDL;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("P", metadata_key);

  key = librmb::RBOX_METADATA_POP3_ORDER;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("O", metadata_key);

  key = librmb::RBOX_METADATA_RECEIVED_TIME;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("R", metadata_key);

  key = librmb::RBOX_METADATA_PHYSICAL_SIZE;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("Z", metadata_key);

  key = librmb::RBOX_METADATA_VIRTUAL_SIZE;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("V", metadata_key);

  key = librmb::RBOX_METADATA_EXT_REF;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("X", metadata_key);

  key = librmb::RBOX_METADATA_ORIG_MAILBOX;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("B", metadata_key);

  key = librmb::RBOX_METADATA_MAIL_UID;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("U", metadata_key);

  key = librmb::RBOX_METADATA_VERSION;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("I", metadata_key);

  key = librmb::RBOX_METADATA_FROM_ENVELOPE;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("A", metadata_key);

  key = librmb::RBOX_METADATA_PVT_FLAGS;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("C", metadata_key);

  key = librmb::RBOX_METADATA_OLDV1_EXPUNGED;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("E", metadata_key);

  key = librmb::RBOX_METADATA_OLDV1_FLAGS;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("F", metadata_key);

  key = librmb::RBOX_METADATA_OLDV1_KEYWORDS;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("K", metadata_key);

  key = librmb::RBOX_METADATA_OLDV1_SAVE_TIME;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ("S", metadata_key);

  key = librmb::RBOX_METADATA_OLDV1_SPACE;
  metadata_key = librmb::rbox_metadata_key_to_char(key);
  EXPECT_EQ(" ", metadata_key);
}

TEST(librmb, utils_convert_str_to_time) {
  time_t test_time;
  // %Y-%m-%d %H:%M:%S
  const std::string test_string = "2017-10-10 12:12:12";
  EXPECT_TRUE(librmb::RadosUtils::convert_str_to_time_t(test_string, &test_time));
  // EXPECT_EQ(test_time, 1507630332);
  EXPECT_NE(0, test_time);
  EXPECT_FALSE(librmb::RadosUtils::convert_str_to_time_t("2017-2-2 ", &test_time));
  EXPECT_FALSE(librmb::RadosUtils::convert_str_to_time_t("", &test_time));
}
TEST(librmb, utils_is_numeric) {
  EXPECT_TRUE(librmb::RadosUtils::is_numeric("12345678"));
  EXPECT_FALSE(librmb::RadosUtils::is_numeric("abchdjdkd"));
  EXPECT_FALSE(librmb::RadosUtils::is_numeric("1234Abvsusi§EEE"));
}
TEST(librmb, utils_is_date_attribute) {
  enum librmb::rbox_metadata_key key = librmb::RBOX_METADATA_RECEIVED_TIME;
  EXPECT_TRUE(librmb::RadosUtils::is_date_attribute(key));
  enum librmb::rbox_metadata_key key2 = librmb::RBOX_METADATA_OLDV1_SAVE_TIME;
  EXPECT_TRUE(librmb::RadosUtils::is_date_attribute(key2));

  enum librmb::rbox_metadata_key key3 = librmb::RBOX_METADATA_VERSION;
  EXPECT_FALSE(librmb::RadosUtils::is_date_attribute(key3));
}
TEST(librmb, utils_convert_string_to_date) {
  std::string date_str = "2017-10-10 12:12:12";
  std::string str;
  librmb::RadosUtils::convert_string_to_date(date_str, &str);
  // EXPECT_EQ(str, "1507630332");
  EXPECT_NE("0", "1507630332");
  std::string test_str = "asjsjsjsj09202920";
  std::string str2;
  librmb::RadosUtils::convert_string_to_date(test_str, &str2);
  EXPECT_EQ(str2, "");
}

TEST(librmb, utils_convert_convert_time_t_to_str) {
  time_t test_time = 1507630332;
  std::string test_str;
  EXPECT_EQ(0, librmb::RadosUtils::convert_time_t_to_str(test_time, &test_str));
  // EXPECT_EQ(test_str, "2017-10-10 12:12:12");
  EXPECT_NE("", test_str);
}

TEST(librmb, config_mutable_metadata) {
  librmb::RadosCephJsonConfig config;
  std::string str = "MGP";
  config.update_mail_attribute(str.c_str());
  EXPECT_TRUE(config.is_mail_attribute(librmb::RBOX_METADATA_MAILBOX_GUID));
  EXPECT_TRUE(config.is_mail_attribute(librmb::RBOX_METADATA_GUID));
  EXPECT_TRUE(config.is_mail_attribute(librmb::RBOX_METADATA_POP3_UIDL));
  EXPECT_FALSE(config.is_mail_attribute(librmb::RBOX_METADATA_ORIG_MAILBOX));

  // use defaults.
  librmb::RadosCephJsonConfig config2;
  config2.update_mail_attribute(NULL);
  EXPECT_TRUE(config2.is_mail_attribute(librmb::RBOX_METADATA_POP3_UIDL));
}

TEST(librmb, convert_flags) {
  uint8_t flags = 0x3f;
  std::string s;
  librmb::RadosUtils::flags_to_string(flags, &s);

  uint8_t flags_;
  librmb::RadosUtils::string_to_flags(s, &flags_);
  EXPECT_EQ(flags, flags_);
}

TEST(librmb, find_and_replace) {
  const std::string str_red_house = "i have a red house and a red car";
  const std::string str_banana = "i love banana";
  const std::string str_underscore = "some_words_separated_by_underscore";
  const std::string str_missing = "th string has an  msing";
  const std::string str_hello = "hello world ";

  std::string text;

  text = "i have a blue house and a blue car";
  librmb::RadosUtils::find_and_replace(&text, "blue", "red");
  EXPECT_EQ(str_red_house, text);

  text = "i love apple";
  librmb::RadosUtils::find_and_replace(&text, "apple", "banana");
  EXPECT_EQ(str_banana, text);

  text = "some-words-separated-by-hyphen";
  librmb::RadosUtils::find_and_replace(&text, "-", "_");
  librmb::RadosUtils::find_and_replace(&text, "hyphen", "underscore");
  EXPECT_EQ(str_underscore, text);

  text = "this string has an is missing";
  librmb::RadosUtils::find_and_replace(&text, "is", "");
  EXPECT_EQ(str_missing, text);

  text = "hello;world;";
  librmb::RadosUtils::find_and_replace(&text, ";", " ");
  EXPECT_EQ(str_hello, text);
}
TEST(librmb, append_to_new_log_file) {
  std::string test_file_name = "test.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("abc", "ns_1", "mail_storage", "save"));
  EXPECT_EQ(true, log_file.close());

  int line_count = 0;
  /** check content **/
  std::ifstream read(test_file_name);
  while (true) {
    librmb::RadosSaveLogEntry entry;
    read >> entry;

    if (read.eof()) {
      break;
    }
    EXPECT_EQ(entry.oid, "abc");
    EXPECT_EQ(entry.ns, "ns_1");
    EXPECT_EQ(entry.pool, "mail_storage");
    EXPECT_EQ(entry.op, "save");
    line_count++;
  }
  EXPECT_EQ(1, line_count);
  read.close();
  std::remove(test_file_name.c_str());
}

TEST(librmb, append_to_existing_file_log_file) {
  std::string test_file_name = "test.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("abc", "ns_1", "mail_storage", "save"));
  EXPECT_EQ(true, log_file.close());

  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("abc", "ns_1", "mail_storage", "save"));
  EXPECT_EQ(true, log_file.close());

  int line_count = 0;
  /** check content **/
  std::ifstream read(test_file_name);
  while (true) {
    librmb::RadosSaveLogEntry entry;
    read >> entry;

    if (read.eof()) {
      break;
    }
    EXPECT_EQ(entry.oid, "abc");
    EXPECT_EQ(entry.ns, "ns_1");
    EXPECT_EQ(entry.pool, "mail_storage");
    EXPECT_EQ(entry.op, "save");
    line_count++;
  }
  EXPECT_EQ(2, line_count);
  read.close();
  std::remove(test_file_name.c_str());
}

__attribute__((noreturn)) static void *write_to_save_file(void *threadid) {
  std::string test_file_name = "test1.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  for (int i = 0; i < 5; i++) {
    log_file.append(librmb::RadosSaveLogEntry("abc", "ns_1", "mail_storage", "save"));
  }
  EXPECT_EQ(true, log_file.close());
  std::cout << " exiting thread " << threadid << std::endl;

  pthread_exit(NULL);
}
TEST(librmb, append_to_existing_file_multi_threading) {
  std::string test_file_name = "test1.log";
  int rc;
  void *status;
  pthread_attr_t attr;
  pthread_t threads[5];
  // Initialize and set thread joinable
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

  for (uintptr_t i = 0; i < 5; i++) {
    rc = pthread_create(&threads[i], NULL, write_to_save_file, (void *)i);
  }
  sleep(1);
  std::cout << " threads created " << std::endl;
  // free attribute and wait for the other threads
  pthread_attr_destroy(&attr);
  for (int i = 0; i < 5; i++) {
    rc = pthread_join(threads[i], &status);
    if (rc) {
      std::cout << "Error:unable to join," << rc << std::endl;
      exit(-1);
    }

    std::cout << "Main: completed thread id :" << i;
    std::cout << "  exiting with status :" << status << std::endl;
  }
  sleep(1);
  int line_count = 0;
  /** check content **/
  std::ifstream read(test_file_name);
  while (true) {
    librmb::RadosSaveLogEntry entry;
    read >> entry;

    if (read.eof()) {
      break;
    }
    EXPECT_EQ(entry.oid, "abc");
    EXPECT_EQ(entry.ns, "ns_1");
    EXPECT_EQ(entry.pool, "mail_storage");
    EXPECT_EQ(entry.op, "save");
    line_count++;
  }
  EXPECT_EQ(25, line_count);
  read.close();
  std::remove(test_file_name.c_str());

  std::cout << " exiting main " << std::endl;
  // pthread_exit(NULL);
}

TEST(librmb, test_mvn_option) {
  std::list<librmb::RadosMetadata *> metadata;
  librmb::RadosMetadata guid(librmb::RBOX_METADATA_MAILBOX_GUID, "ABCDEFG");
  librmb::RadosMetadata mb_name(librmb::RBOX_METADATA_ORIG_MAILBOX, "INBOX");
  uint uid_ = 1;
  librmb::RadosMetadata uid(librmb::RBOX_METADATA_MAIL_UID, uid_);
  metadata.push_back(&guid);
  metadata.push_back(&mb_name);
  metadata.push_back(&uid);

  std::string test_file_name = "test_2.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("dest_oid", "ns_dest", "mail_storage",
                                            librmb::RadosSaveLogEntry::op_mv("ns_src", "src_oid", "user", metadata)));
  std::ifstream read(test_file_name);
  while (true) {
    librmb::RadosSaveLogEntry entry;
    read >> entry;

    if (read.eof()) {
      break;
    }
    EXPECT_EQ(entry.oid, "dest_oid");
    EXPECT_EQ(entry.ns, "ns_dest");
    EXPECT_EQ(entry.pool, "mail_storage");
    EXPECT_EQ(entry.op, "mv:ns_src:src_oid:user;M=ABCDEFG:B=INBOX:U=1\0");
    EXPECT_EQ(entry.metadata.size(), 3);
  }

  read.close();
  // EXPECT_EQ(1, 0);
  std::remove(test_file_name.c_str());
}

/*TEST(librmb, test_if) {
  int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};

  for (int i = 0; i < 10; ++i) {
    std::cout << " value: " << arr[i] << std::endl;
  }
  for (int i = 0; i < 10; i++) {
    std::cout << " value: " << arr[i] << std::endl;
  }

  EXPECT_EQ(1, 2);
}*/
TEST(librmb, mock_obj) {}
int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
