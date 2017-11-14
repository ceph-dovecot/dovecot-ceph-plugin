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

#include <ctime>
#include <rados/librados.hpp>

#include "../../librmb/rados-cluster-impl.h"
#include "../../librmb/rados-storage-impl.h"
#include "mock_test.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "rados-util.h"
#include "rados-config.h"
#include "rados-types.h"

using ::testing::AtLeast;
using ::testing::Return;

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
  std::string str = librmb::RadosUtils::convert_string_to_date(date_str);
  // EXPECT_EQ(str, "1507630332");
  EXPECT_NE("0", "1507630332");
  std::string test_str = "asjsjsjsj09202920";
  std::string str2 = librmb::RadosUtils::convert_string_to_date(test_str);
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
  librmb::RadosConfig config;
  std::string str = "MGP";
  config.update_mutable_metadata(str.c_str());
  EXPECT_TRUE(config.is_mutable_metadata(librmb::RBOX_METADATA_MAILBOX_GUID));
  EXPECT_TRUE(config.is_mutable_metadata(librmb::RBOX_METADATA_GUID));
  EXPECT_TRUE(config.is_mutable_metadata(librmb::RBOX_METADATA_POP3_UIDL));
  EXPECT_FALSE(config.is_mutable_metadata(librmb::RBOX_METADATA_ORIG_MAILBOX));

  // use defaults.
  librmb::RadosConfig config2;
  config2.update_mutable_metadata(NULL);
  EXPECT_TRUE(config2.is_mutable_metadata(librmb::RBOX_METADATA_POP3_UIDL));
}

TEST(librmb, convert_flags) {
  uint8_t flags = 0x3f;
  std::string s = librmb::RadosUtils::flags_to_string(flags);

  uint8_t flags_ = librmb::RadosUtils::string_to_flags(s);
  EXPECT_EQ(flags, flags_);
}

TEST(librmb, mock_obj) {}
int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
