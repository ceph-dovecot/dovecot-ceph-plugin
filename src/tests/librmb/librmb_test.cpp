/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <ctime>
#include <rados/librados.hpp>

#include "gtest/gtest.h"
#include "rados-storage.h"
#include "rados-cluster.h"

TEST(librmb, split_write_operation) {
  const char *buffer = "abcdefghijklmn";
  size_t buffer_length = 14;
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;
  librmb::RadosStorage *storage = NULL;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosCluster cluster;

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = cluster.open_connection(&storage, pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage->split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  obj.wait_for_write_operations_complete();

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage->get_io_ctx().stat(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage->get_io_ctx().remove(obj.get_oid());

  // tear down
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(5, obj.get_completion_op_map()->size());
}

TEST(librmb1, split_write_operation_1) {
  const char *buffer = "HALLO_WELT_";
  size_t buffer_length = 11;
  uint64_t max_size = buffer_length;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;
  librmb::RadosStorage *storage = NULL;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosCluster cluster;

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = cluster.open_connection(&storage, pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage->split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  obj.wait_for_write_operations_complete();

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage->get_io_ctx().stat(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage->get_io_ctx().remove(obj.get_oid());

  // tear down.
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(1, obj.get_completion_op_map()->size());
}

TEST(librmb1, convert_types) {
  librmb::RadosXAttr attr;
  std::string value = "4441c5339f4c9d59523000009c60b9f7";
  librmb::RadosXAttr::convert(librmb::RBOX_METADATA_GUID, value, &attr);
  EXPECT_EQ(attr.key, "G");
  EXPECT_EQ(attr.bl.to_str(), value);
  time_t t = 1503488583;

  attr.key = "";
  attr.bl.clear();
  librmb::RadosXAttr::convert(librmb::RBOX_METADATA_RECEIVED_TIME, t, &attr);
  EXPECT_EQ(attr.key, "R");
  EXPECT_EQ(attr.bl.to_str(), "1503488583");

  time_t recv_date;
  librmb::RadosXAttr::convert(attr.bl.to_str().c_str(), &recv_date);
  EXPECT_EQ(t, recv_date);

  attr.key = "";
  attr.bl.clear();
  size_t st = 100;
  librmb::RadosXAttr::convert(librmb::RBOX_METADATA_VIRTUAL_SIZE, st, &attr);
  EXPECT_EQ(attr.key, "V");
  EXPECT_EQ(attr.bl.to_str(), "100");

  attr.key = "";
  attr.bl.clear();
}

TEST(librmb1, read_mail) {
  const char *buffer = "abcdefghijklmn";
  size_t buffer_length = 14;
  uint64_t max_size = buffer_length;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;
  librmb::RadosStorage *storage = NULL;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosCluster cluster;

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = cluster.open_connection(&storage, pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage->split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  obj.wait_for_write_operations_complete();

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage->get_io_ctx().stat(obj.get_oid(), &size, &save_date);

  char *buff = new char[size];
  int ret = storage->read_mail(obj.get_oid(), &size, &buff[0]);

  // remove it
  int ret_remove = storage->get_io_ctx().remove(obj.get_oid());

  // tear down
  cluster.deinit();
  EXPECT_EQ(ret_storage, 0);
  EXPECT_EQ(ret_stat, 0);
  EXPECT_EQ(ret_remove, 0);
  EXPECT_EQ(ret, 0);
  EXPECT_EQ(buff[0], 'a');
  EXPECT_EQ(buff[1], 'b');
  EXPECT_EQ(buff[2], 'c');
  EXPECT_EQ(buff[3], 'd');
}

TEST(librmb, load_xattr) {
  const char *buffer = "abcdefghijklmn";
  size_t buffer_length = 14;
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;
  librmb::RadosStorage *storage = NULL;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosCluster cluster;

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = cluster.open_connection(&storage, pool_name, ns);
  EXPECT_EQ(0, open_connection);

  ceph::bufferlist bl;
  bl.append("xyz");
  op->setxattr("A", bl);
  op->setxattr("B", bl);

  int ret_storage = storage->split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  // wait for op to finish.
  obj.wait_for_write_operations_complete();

  storage->load_xattr(&obj);

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage->get_io_ctx().stat(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage->get_io_ctx().remove(obj.get_oid());

  // tear down
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(5, obj.get_completion_op_map()->size());
  EXPECT_EQ(2, obj.get_xattr()->size());

  int i = storage->load_xattr(nullptr);
  EXPECT_EQ(-1, i);

  i = storage->load_xattr(&obj);
  EXPECT_EQ(0, i);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
