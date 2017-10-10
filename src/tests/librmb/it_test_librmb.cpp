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

using ::testing::AtLeast;
using ::testing::Return;

TEST(librmb, mock_test) {
  librmbtest::RadosClusterMock cluster;
  librmbtest::RadosStorageMock storage;  // = new RadosStorageMock();
  EXPECT_CALL(storage, get_max_write_size()).Times(AtLeast(1)).WillOnce(Return(100));
  int i = storage.get_max_write_size();
  EXPECT_EQ(i, 100);
}

TEST(librmb, split_write_operation) {
  const char *buffer = "abcdefghijklmn";
  size_t buffer_length = 14;
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage.split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage.stat_mail(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage.delete_mail(&obj);

  // tear down
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(5, (int)obj.get_completion_op_map()->size());
}

TEST(librmb1, split_write_operation_1) {
  const char *buffer = "HALLO_WELT_";
  size_t buffer_length = 11;
  uint64_t max_size = buffer_length;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage.split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage.stat_mail(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage.delete_mail(obj.get_oid());

  // tear down.
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(1, (int)obj.get_completion_op_map()->size());
}

TEST(librmb1, convert_types) {
  std::string value = "4441c5339f4c9d59523000009c60b9f7";
  librmb::RadosMetadata attr(librmb::RBOX_METADATA_GUID, value);

  EXPECT_EQ(attr.key, "G");
  EXPECT_EQ(attr.bl.to_str(), value);
  time_t t = 1503488583;

  attr.key = "";
  attr.bl.clear();
  librmb::RadosMetadata attr2(librmb::RBOX_METADATA_RECEIVED_TIME, t);

  EXPECT_EQ(attr2.key, "R");
  EXPECT_EQ(attr2.bl.to_str(), "1503488583");

  time_t recv_date;
  attr2.convert(attr2.bl.to_str().c_str(), &recv_date);
  EXPECT_EQ(t, recv_date);

  size_t st = 100;
  librmb::RadosMetadata attr4(librmb::RBOX_METADATA_VIRTUAL_SIZE, st);
  EXPECT_EQ(attr4.key, "V");
  EXPECT_EQ(attr4.bl.to_str(), "100");

  attr4.key = "";
  attr4.bl.clear();
}

TEST(librmb1, read_mail) {
  const char *buffer = "abcdefghijklmn";
  size_t buffer_length = 14;
  uint64_t max_size = buffer_length;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage.split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage.stat_mail(obj.get_oid(), &size, &save_date);

  librados::bufferlist bl;
  int copy_mail_ret = storage.read_mail(&bl, obj.get_oid());
  char *buff = new char[copy_mail_ret + 1];
  memset(buff, 1, size + 1);
  memcpy(buff, bl.to_str().c_str(), copy_mail_ret + 1);
  EXPECT_EQ(buff[copy_mail_ret], '\0');

  // remove it
  int ret_remove = storage.delete_mail(obj.get_oid());

  // tear down
  cluster.deinit();
  EXPECT_EQ(ret_storage, 0);
  EXPECT_EQ(ret_stat, 0);
  EXPECT_EQ(ret_remove, 0);
  EXPECT_EQ(copy_mail_ret, 14);
  EXPECT_EQ(buff[0], 'a');
  EXPECT_EQ(buff[1], 'b');
  EXPECT_EQ(buff[2], 'c');
  EXPECT_EQ(buff[3], 'd');

  delete[] buff;
}

TEST(librmb, load_metadata) {
  const char *buffer = "abcdefghijklmn";
  size_t buffer_length = 14;
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name, ns);
  EXPECT_EQ(0, open_connection);

  ceph::bufferlist bl;
  bl.append("xyz");
  op->setxattr("A", bl);
  op->setxattr("B", bl);

  int ret_storage = storage.split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  storage.load_metadata(&obj);

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage.stat_mail(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage.delete_mail(obj.get_oid());

  // tear down
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(5, (int)obj.get_completion_op_map()->size());
  EXPECT_EQ(2, (int)obj.get_metadata()->size());

  int i = storage.load_metadata(nullptr);
  EXPECT_EQ(-1, i);

  i = storage.load_metadata(&obj);
  EXPECT_EQ(0, i);
}
TEST(librmb, mock_obj) {}
int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
