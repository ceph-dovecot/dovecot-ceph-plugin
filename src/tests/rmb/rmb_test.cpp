/*
 * rmb_test.cpp
 *
 *  Created on: Aug 22, 2017
 *      Author: jan
 */

#include "gtest/gtest.h"
#include "rados-storage.h"
#include "rados-cluster.h"
#include "rados-mail-object.h"
#include <rados/librados.hpp>
#include "../../librmb/tools/rmb/ls_cmd_parser.h"

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

  librmb::rbox_metadata_key k = 'M';

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
  p->convert_str_to_time_t(date, t);
  std::cout << "time t " << t << std::endl;
  EXPECT_TRUE(t > -1);
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
