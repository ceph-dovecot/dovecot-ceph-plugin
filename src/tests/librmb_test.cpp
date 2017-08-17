
#include "gtest/gtest.h"
#include "rados-storage.h"
#include "rados-cluster.h"
#include <rados/librados.hpp>

TEST(librmb, split_write_operation) {
  librados::ObjectWriteOperation *write_op_xattr = new librados::ObjectWriteOperation();
  const char *buffer = "abcdefghijklmn";
  size_t buffer_length = 14;
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;
  librmb::RadosStorage *storage;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosCluster cluster;

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = cluster.open_connection(&storage, pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage->split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  obj.wait_for_write_operations_complete();

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage->get_io_ctx().stat(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage->get_io_ctx().remove(obj.get_oid());

  // wait for op to finish.
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(5, obj.get_completion_op_map()->size());
}

TEST(librmb1, split_write_operation_1) {
  librados::ObjectWriteOperation *write_op_xattr = new librados::ObjectWriteOperation();
  const char *buffer = "HALLO_WELT_";
  size_t buffer_length = 11;
  uint64_t max_size = buffer_length;
  librmb::RadosMailObject obj;
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;
  librmb::RadosStorage *storage;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosCluster cluster;

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = cluster.open_connection(&storage, pool_name, ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage->split_buffer_and_exec_op(buffer, buffer_length, &obj, op, max_size);

  obj.wait_for_write_operations_complete();

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage->get_io_ctx().stat(obj.get_oid(), &size, &save_date);

  // remove it
  int ret_remove = storage->get_io_ctx().remove(obj.get_oid());

  // wait for op to finish.
  cluster.deinit();

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(0, ret_remove);
  EXPECT_EQ(1, obj.get_completion_op_map()->size());
}
