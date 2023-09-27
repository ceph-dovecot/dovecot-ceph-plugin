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

#include "../storage-mock-rbox/TestCase.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"           // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wundef"            // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wredundant-decls"  // turn off warnings for Dovecot :-(
#ifndef __cplusplus
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"  // turn off warnings for Dovecot :-(
#endif

extern "C" {
#include "lib.h"
#include "mail-user.h"
#include "mail-storage.h"
#include "mail-storage-service.h"
#include "mail-namespace.h"
#include "mailbox-list.h"
#include "ioloop.h"
#include "istream.h"
#include "mail-search-build.h"
#include "ostream.h"
#include "libdict-rados-plugin.h"
}


#include "dovecot-ceph-plugin-config.h"
#include "../test-utils/it_utils.h"

#include "rbox-storage.hpp"
#include "rbox-save.h"

#include "../mocks/mock_test.h"
#include "rados-util.h"

using ::testing::_;
using ::testing::AtLeast;
using ::testing::Matcher;
using ::testing::Return;
using ::testing::ReturnRef;
#pragma GCC diagnostic pop

#if DOVECOT_PREREQ(2, 3, 0)
#define mailbox_get_last_internal_error(box, error_r) mailbox_get_last_internal_error(box, error_r)
#else
#define mailbox_get_last_internal_error(box, error_r) mailbox_get_last_error(box, error_r)
#endif

#ifndef i_zero
#define i_zero(p) memset(p, 0, sizeof(*(p)))
#endif

/**
 * Parse ls-by-pool output command for pg ids
 *
 */
TEST_F(StorageTest, ParseLsByPoolOutput) {
  
  const std::string lsByPoolOutPut =
      "mon command outbl: PG     OBJECTS  DEGRADED  MISPLACED  UNFOUND  BYTES      OMAP_BYTES*  OMAP_KEYS*  LOG   STATE         SINCE  VERSION      REPORTED     UP         ACTING     SCRUB_STAMP                      DEEP_SCRUB_STAMP\n\
  10.0       568         0          0        0     841115            0           0  1165  active+clean    26h   3641'18906   3656:51938  [3,1,4]p3  [3,1,4]p3  2022-09-12T15:41:31.269234+0000  2022-09-12T15:41:31.269234+0000\n\
  10.1       550         0          0        0   33248442            0           0  1167  active+clean    25h   3641'22959   3656:56961  [3,2,4]p3  [3,2,4]p3  2022-09-12T17:02:53.075147+0000  2022-09-12T17:02:53.075147+0000\n\
  10.2       597         0          0        0    1159934            0           0  1323  active+clean    14h   3641'28591   3656:60048  [4,3,2]p4  [4,3,2]p4  2022-09-13T04:19:15.413172+0000  2022-09-11T22:05:48.081770+0000\n\
  10.3       604         0          0        0   33341614            0           0  1438  active+clean     2h   3641'22933   3657:55462  [1,3,2]p1  [1,3,2]p1  2022-09-13T16:08:56.004228+0000  2022-09-11T09:09:04.723301+0000\n\
  10.4       562         0          0        0     868945            0           0  1379  active+clean     4h   3641'20673   3656:52381  [1,4,3]p1  [1,4,3]p1  2022-09-13T14:36:42.995722+0000  2022-09-13T14:36:42.995722+0000\n\
  10.5       603         0          0        0  158821233            0           0  1167  active+clean    18h  3641'117859  3656:149450  [3,4,2]p3  [3,4,2]p3  2022-09-13T00:09:30.875802+0000  2022-09-11T17:26:47.059142+0000\n\
  10.6       644         0          0        0     933092            0           0  1292  active+clean    43m   3641'22101   3657:54557  [2,1,3]p2  [2,1,3]p2  2022-09-13T17:57:17.071685+0000  2022-09-13T17:57:17.071685+0000\n\
  10.7       626         0          0        0   95786518            0           0  1193  active+clean     3h   3641'20186   3657:54334  [3,4,1]p3  [3,4,1]p3  2022-09-13T15:22:51.015523+0000  2022-09-12T14:50:10.304250+0000\n\
  10.8       590         0          0        0    1360275            0           0  1150  active+clean     3h   3641'19029   3657:49696  [3,1,2]p3  [3,1,2]p3  2022-09-13T15:22:28.040315+0000  2022-09-07T06:21:41.088829+0000\n\
  10.9       575         0          0        0     845156            0           0  1314  active+clean    16h   3641'19322   3656:50842  [2,4,3]p2  [2,4,3]p2  2022-09-13T01:52:08.817572+0000  2022-09-13T01:52:08.817572+0000\n\
  10.a       567         0          0        0     805954            0           0  1219  active+clean    25h   3641'37553   3656:68282  [3,1,2]p3  [3,1,2]p3  2022-09-12T16:42:57.697431+0000  2022-09-10T10:07:31.790317+0000\n\
  10.b       582         0          0        0     885900            0           0  1298  active+clean    25h   3641'19280   3656:52838  [2,4,3]p2  [2,4,3]p2  2022-09-12T16:46:16.220323+0000  2022-09-06T17:38:36.977892+0000\n\
  10.c       549         0          0        0     751071            0           0  1304  active+clean    11h   3641'19066   3656:50539  [2,3,4]p2  [2,3,4]p2  2022-09-13T06:44:32.983447+0000  2022-09-07T18:30:45.260115+0000\n\
  10.d       650         0          0        0     933800            0           0  1250  active+clean    23h  3641'103478  3656:135352  [2,4,3]p2  [2,4,3]p2  2022-09-12T18:48:06.113121+0000  2022-09-06T20:18:07.792822+0000\n\
  10.e       566         0          0        0     851780            0           0  1249  active+clean     2h   3532'19569   3657:51205  [2,4,3]p2  [2,4,3]p2  2022-09-13T16:13:32.829316+0000  2022-09-08T14:10:17.974007+0000\n\
  10.f       597         0          0        0     826336            0           0  1169  active+clean    13h   3641'18988   3656:50480  [3,1,2]p3  [3,1,2]p3  2022-09-13T05:09:49.851413+0000  2022-09-11T19:36:45.701069+0000\n\
  10.10      562         0          0        0     862878            0           0  1192  active+clean    13h   3641'19245  3656:318916  [3,2,4]p3  [3,2,4]p3  2022-09-13T05:29:59.129259+0000  2022-09-13T05:29:59.129259+0000\n\
  10.11      547         0          0        0     797004            0           0  1372  active+clean    16h   3641'18587   3656:50156  [1,4,2]p1  [1,4,2]p1  2022-09-13T02:40:21.127749+0000  2022-09-08T08:06:48.604409+0000\n\
  10.12      530         0          0        0    1052374            0           0  1155  active+clean    28h   3641'20746   3656:51482  [3,1,2]p3  [3,1,2]p3  2022-09-12T14:24:27.828869+0000  2022-09-11T13:33:05.018892+0000\n\
  10.13      572         0          0        0     847499            0           0  1346  active+clean    15h   3641'28749   3656:60112  [4,3,2]p4  [4,3,2]p4  2022-09-13T03:01:34.151958+0000  2022-09-10T16:11:36.949992+0000\n\
  10.14      599         0          0        0    1169276            0           0  1279  active+clean     9h   3641'20130   3656:50579  [4,3,1]p4  [4,3,1]p4  2022-09-13T08:48:29.616926+0000  2022-09-13T08:48:29.616926+0000\n\
  10.15      586         0          0        0     866197            0           0  1279  active+clean    13h  3641'116658  3656:147540  [4,3,2]p4  [4,3,2]p4  2022-09-13T05:36:45.359570+0000  2022-09-08T08:10:42.049640+0000\n\
  10.16      587         0          0        0    1227527            0           0  1256  active+clean    27h   3641'19293   3656:51660  [2,4,3]p2  [2,4,3]p2  2022-09-12T15:19:09.641685+0000  2022-09-08T21:50:48.032555+0000\n\
  10.17      566         0          0        0  158772753            0           0  1183  active+clean     7h   3641'34532   3656:65851  [3,1,2]p3  [3,1,2]p3  2022-09-13T10:47:45.614204+0000  2022-09-12T01:21:42.220916+0000\n\
  10.18      559         0          0        0   63859162            0           0  1354  active+clean    23h   3641'25544   3656:57788  [4,1,2]p4  [4,1,2]p4  2022-09-12T19:35:08.476390+0000  2022-09-11T15:04:20.133155+0000\n\
  10.19      613         0          0        0     842769            0           0  1166  active+clean    30h   3641'38205   3656:71482  [3,2,4]p3  [3,2,4]p3  2022-09-12T12:21:42.007023+0000  2022-09-06T01:01:59.844062+0000\n\
  10.1a      587         0          0        0     845231            0           0  1250  active+clean    29h   3641'24127   3656:55136  [2,3,1]p2  [2,3,1]p2  2022-09-12T12:52:01.529860+0000  2022-09-11T07:24:15.003205+0000\n\
  10.1b      549         0          0        0   76692836            0           0  1316  active+clean    25h   3641'26875   3656:57767  [4,2,3]p4  [4,2,3]p4  2022-09-12T17:06:52.309745+0000  2022-09-08T05:40:28.503848+0000\n\
  10.1c      604         0          0        0    1114372            0           0  1329  active+clean    62m   3641'18755   3657:50579  [2,3,1]p2  [2,3,1]p2  2022-09-13T17:37:36.331891+0000  2022-09-13T17:37:36.331891+0000\n\
  10.1d      607         0          0        0  191148409            0           0  1271  active+clean    26h   3641'19526   3656:51525  [2,1,3]p2  [2,1,3]p2  2022-09-12T16:25:02.581960+0000  2022-09-12T16:25:02.581960+0000\n\
  10.1e      593         0          0        0     848897            0           0  1413  active+clean    20h   3641'19318   3656:51479  [1,3,4]p1  [1,3,4]p1  2022-09-12T22:13:27.455449+0000  2022-09-12T22:13:27.455449+0000\n\
  10.1f      589         0          0        0     827645            0           0  1288  active+clean    13h   3641'20726   3656:53355  [2,4,1]p2  [2,4,1]p2  2022-09-13T05:09:00.823199+0000  2022-09-07T23:42:54.081441+0000\n";

  std::vector<std::string> list = librmb::RadosUtils::extractPgs(lsByPoolOutPut);
    
  /*for (auto const &token: list) {
        std::cout << token << std::endl;        
  }*/

  EXPECT_EQ("10.0", list[0]);
  EXPECT_EQ("10.1",list[1]);
  EXPECT_EQ("10.2",list[2]);
  EXPECT_EQ("10.3",list[3]);
  EXPECT_EQ("10.4",list[4]);
  EXPECT_EQ("10.5",list[5]);
  EXPECT_EQ("10.6",list[6]);
  EXPECT_EQ("10.7",list[7]);
  EXPECT_EQ("10.8",list[8]);
  EXPECT_EQ("10.9",list[9]);
  EXPECT_EQ("10.a",list[10]);
  EXPECT_EQ("10.b",list[11]);
  EXPECT_EQ("10.c",list[12]);
  EXPECT_EQ("10.d",list[13]);
  EXPECT_EQ("10.e",list[14]);
  EXPECT_EQ("10.f",list[15]);
  EXPECT_EQ("10.10",list[16]);
  EXPECT_EQ("10.11",list[17]);
  EXPECT_EQ("10.12",list[18]);
  EXPECT_EQ("10.13",list[19]);
  EXPECT_EQ("10.14",list[20]);
  EXPECT_EQ("10.15",list[21]);
  EXPECT_EQ("10.16",list[22]);
  EXPECT_EQ("10.17",list[23]);
  EXPECT_EQ("10.18",list[24]);
  EXPECT_EQ("10.19",list[25]);
  EXPECT_EQ("10.1a",list[26]);
  EXPECT_EQ("10.1b",list[27]);
  EXPECT_EQ("10.1c",list[28]);
  EXPECT_EQ("10.1d",list[29]);
  EXPECT_EQ("10.1e",list[30]);
  EXPECT_EQ("10.1f",list[31]);

}
TEST_F(StorageTest, scanForPg) {
  
  const std::string header =
      "mon command outbl: PG     OBJECTS  DEGRADED  MISPLACED  UNFOUND  BYTES      OMAP_BYTES*  OMAP_KEYS*  LOG   STATE         SINCE  VERSION      REPORTED     UP         ACTING     SCRUB_STAMP                      DEEP_SCRUB_STAMP\n";

  const std::string row ="10.0       568         0          0        0     841115            0           0  1165  active+clean    26h   3641'18906   3656:51938  [3,1,4]p3  [3,1,4]p3  2022-09-12T15:41:31.269234+0000  2022-09-12T15:41:31.269234+0000\n";

  std::vector<std::string> list = librmb::RadosUtils::split(row,' ');
  EXPECT_EQ(17,list.size());

  std::vector<std::string> list2= librmb::RadosUtils::split(header,' ');
  EXPECT_EQ(20,list2.size());

}
TEST_F(StorageTest, extractPrimaryOsd) {
  
    const std::string lsByPoolOutPut =
        "mon command outbl: PG     OBJECTS  DEGRADED  MISPLACED  UNFOUND  BYTES      OMAP_BYTES*  OMAP_KEYS*  LOG   STATE         SINCE  VERSION      REPORTED     UP         ACTING     SCRUB_STAMP                      DEEP_SCRUB_STAMP\n\
    10.0       568         0          0        0     841115            0           0  1165  active+clean    26h   3641'18906   3656:51938  [3,1,4]p3  [3,1,4]p3  2022-09-12T15:41:31.269234+0000  2022-09-12T15:41:31.269234+0000\n\
    10.1       550         0          0        0   33248442            0           0  1167  active+clean    25h   3641'22959   3656:56961  [3,2,4]p3  [3,2,4]p3  2022-09-12T17:02:53.075147+0000  2022-09-12T17:02:53.075147+0000\n\
    10.2       597         0          0        0    1159934            0           0  1323  active+clean    14h   3641'28591   3656:60048  [4,3,2]p4  [4,3,2]p4  2022-09-13T04:19:15.413172+0000  2022-09-11T22:05:48.081770+0000\n\
    10.3       604         0          0        0   33341614            0           0  1438  active+clean     2h   3641'22933   3657:55462  [1,3,2]p1  [1,3,2]p1  2022-09-13T16:08:56.004228+0000  2022-09-11T09:09:04.723301+0000\n\
    10.4       562         0          0        0     868945            0           0  1379  active+clean     4h   3641'20673   3656:52381  [1,4,3]p1  [1,4,3]p1  2022-09-13T14:36:42.995722+0000  2022-09-13T14:36:42.995722+0000\n\
    10.5       603         0          0        0  158821233            0           0  1167  active+clean    18h  3641'117859  3656:149450  [3,4,2]p3  [3,4,2]p3  2022-09-13T00:09:30.875802+0000  2022-09-11T17:26:47.059142+0000\n\
    10.6       644         0          0        0     933092            0           0  1292  active+clean    43m   3641'22101   3657:54557  [2,1,3]p2  [2,1,3]p2  2022-09-13T17:57:17.071685+0000  2022-09-13T17:57:17.071685+0000\n\
    10.7       626         0          0        0   95786518            0           0  1193  active+clean     3h   3641'20186   3657:54334  [3,4,1]p3  [3,4,1]p3  2022-09-13T15:22:51.015523+0000  2022-09-12T14:50:10.304250+0000\n\
    10.8       590         0          0        0    1360275            0           0  1150  active+clean     3h   3641'19029   3657:49696  [3,1,2]p3  [3,1,2]p3  2022-09-13T15:22:28.040315+0000  2022-09-07T06:21:41.088829+0000\n\
    10.9       575         0          0        0     845156            0           0  1314  active+clean    16h   3641'19322   3656:50842  [2,4,3]p2  [2,4,3]p2  2022-09-13T01:52:08.817572+0000  2022-09-13T01:52:08.817572+0000\n\
    10.a       567         0          0        0     805954            0           0  1219  active+clean    25h   3641'37553   3656:68282  [3,1,2]p3  [3,1,2]p3  2022-09-12T16:42:57.697431+0000  2022-09-10T10:07:31.790317+0000\n\
    10.b       582         0          0        0     885900            0           0  1298  active+clean    25h   3641'19280   3656:52838  [2,4,3]p2  [2,4,3]p2  2022-09-12T16:46:16.220323+0000  2022-09-06T17:38:36.977892+0000\n\
    10.c       549         0          0        0     751071            0           0  1304  active+clean    11h   3641'19066   3656:50539  [2,3,4]p2  [2,3,4]p2  2022-09-13T06:44:32.983447+0000  2022-09-07T18:30:45.260115+0000\n\
    10.d       650         0          0        0     933800            0           0  1250  active+clean    23h  3641'103478  3656:135352  [2,4,3]p2  [2,4,3]p2  2022-09-12T18:48:06.113121+0000  2022-09-06T20:18:07.792822+0000\n\
    10.e       566         0          0        0     851780            0           0  1249  active+clean     2h   3532'19569   3657:51205  [2,4,3]p2  [2,4,3]p2  2022-09-13T16:13:32.829316+0000  2022-09-08T14:10:17.974007+0000\n\
    10.f       597         0          0        0     826336            0           0  1169  active+clean    13h   3641'18988   3656:50480  [3,1,2]p3  [3,1,2]p3  2022-09-13T05:09:49.851413+0000  2022-09-11T19:36:45.701069+0000\n\
    10.10      562         0          0        0     862878            0           0  1192  active+clean    13h   3641'19245  3656:318916  [3,2,4]p3  [3,2,4]p3  2022-09-13T05:29:59.129259+0000  2022-09-13T05:29:59.129259+0000\n\
    10.11      547         0          0        0     797004            0           0  1372  active+clean    16h   3641'18587   3656:50156  [1,4,2]p1  [1,4,2]p1  2022-09-13T02:40:21.127749+0000  2022-09-08T08:06:48.604409+0000\n\
    10.12      530         0          0        0    1052374            0           0  1155  active+clean    28h   3641'20746   3656:51482  [3,1,2]p3  [3,1,2]p3  2022-09-12T14:24:27.828869+0000  2022-09-11T13:33:05.018892+0000\n\
    10.13      572         0          0        0     847499            0           0  1346  active+clean    15h   3641'28749   3656:60112  [4,3,2]p4  [4,3,2]p4  2022-09-13T03:01:34.151958+0000  2022-09-10T16:11:36.949992+0000\n\
    10.14      599         0          0        0    1169276            0           0  1279  active+clean     9h   3641'20130   3656:50579  [4,3,1]p4  [4,3,1]p4  2022-09-13T08:48:29.616926+0000  2022-09-13T08:48:29.616926+0000\n\
    10.15      586         0          0        0     866197            0           0  1279  active+clean    13h  3641'116658  3656:147540  [4,3,2]p4  [4,3,2]p4  2022-09-13T05:36:45.359570+0000  2022-09-08T08:10:42.049640+0000\n\
    10.16      587         0          0        0    1227527            0           0  1256  active+clean    27h   3641'19293   3656:51660  [2,4,3]p2  [2,4,3]p2  2022-09-12T15:19:09.641685+0000  2022-09-08T21:50:48.032555+0000\n\
    10.17      566         0          0        0  158772753            0           0  1183  active+clean     7h   3641'34532   3656:65851  [3,1,2]p3  [3,1,2]p3  2022-09-13T10:47:45.614204+0000  2022-09-12T01:21:42.220916+0000\n\
    10.18      559         0          0        0   63859162            0           0  1354  active+clean    23h   3641'25544   3656:57788  [4,1,2]p4  [4,1,2]p4  2022-09-12T19:35:08.476390+0000  2022-09-11T15:04:20.133155+0000\n\
    10.19      613         0          0        0     842769            0           0  1166  active+clean    30h   3641'38205   3656:71482  [3,2,4]p3  [3,2,4]p3  2022-09-12T12:21:42.007023+0000  2022-09-06T01:01:59.844062+0000\n\
    10.1a      587         0          0        0     845231            0           0  1250  active+clean    29h   3641'24127   3656:55136  [2,3,1]p2  [2,3,1]p2  2022-09-12T12:52:01.529860+0000  2022-09-11T07:24:15.003205+0000\n\
    10.1b      549         0          0        0   76692836            0           0  1316  active+clean    25h   3641'26875   3656:57767  [4,2,3]p4  [4,2,3]p4  2022-09-12T17:06:52.309745+0000  2022-09-08T05:40:28.503848+0000\n\
    10.1c      604         0          0        0    1114372            0           0  1329  active+clean    62m   3641'18755   3657:50579  [2,3,1]p2  [2,3,1]p2  2022-09-13T17:37:36.331891+0000  2022-09-13T17:37:36.331891+0000\n\
    10.1d      607         0          0        0  191148409            0           0  1271  active+clean    26h   3641'19526   3656:51525  [2,1,3]p2  [2,1,3]p2  2022-09-12T16:25:02.581960+0000  2022-09-12T16:25:02.581960+0000\n\
    10.1e      593         0          0        0     848897            0           0  1413  active+clean    20h   3641'19318   3656:51479  [1,3,4]p1  [1,3,4]p1  2022-09-12T22:13:27.455449+0000  2022-09-12T22:13:27.455449+0000\n\
    10.1f      589         0          0        0     827645            0           0  1288  active+clean    13h   3641'20726   3656:53355  [2,4,1]p2  [2,4,1]p2  2022-09-13T05:09:00.823199+0000  2022-09-07T23:42:54.081441+0000\n";

    std::map<std::string,std::vector<std::string>> list = librmb::RadosUtils::extractPgAndPrimaryOsd(lsByPoolOutPut);
      
    for (const auto& x : list)
    {
      std::cout << "first: " << x.first << ", second: " << x.second.size() << std::endl;
    }

    EXPECT_EQ(4,list["1"].size());
    EXPECT_EQ(11,list["2"].size());
    EXPECT_EQ(11,list["3"].size());
    EXPECT_EQ(6,list["4"].size());

}

TEST_F(StorageTest,create_read_index) {

  std::set<std::string> test_mails;
  test_mails.insert("1");
  test_mails.insert("2");
  test_mails.insert("3");
  test_mails.insert("4");
  test_mails.insert("5");
  std::string test_string = librmb::RadosUtils::convert_to_ceph_index(test_mails);
  EXPECT_EQ("1,2,3,4,5,",test_string);

  std::set<std::string> test_mails_read = librmb::RadosUtils::ceph_index_to_set(test_string);
  EXPECT_EQ(5,test_mails_read.size());
  auto my_vect = std::vector<std::string>(test_mails_read.begin(), test_mails_read.end()); // O[n]

  EXPECT_EQ("1",my_vect[0]);
  EXPECT_EQ("2",my_vect[1]);
  EXPECT_EQ("3",my_vect[2]);
  EXPECT_EQ("4",my_vect[3]);
  EXPECT_EQ("5",my_vect[4]);
  

  std::string test_string1 = librmb::RadosUtils::convert_to_ceph_index("abd");
  EXPECT_EQ("abd,",test_string1);


}



int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
