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
#include "../../librmb/rados-storage-impl.h"
#include "mock_test.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "../../librmb/rados-metadata-storage-default.h"
#include "../../librmb/rados-metadata-storage-ima.h"
#include "../../librmb/rados-dovecot-ceph-cfg-impl.h"
#include "../../librmb/rados-util.h"
//#include "common/Formatter.h"
//#include "common/ceph_json.h"

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
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);

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
  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("HALLO_WELT_");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  uint64_t max_size = buffer_length;

  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);

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
  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  uint64_t max_size = buffer_length;

  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage.stat_mail(obj.get_oid(), &size, &save_date);

  librados::bufferlist bl;
  int copy_mail_ret = storage.read_mail(obj.get_oid(), &bl);
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
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);

  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);
  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosMetadataStorageDefault ms(&storage.get_io_ctx());

  ceph::bufferlist bl;
  bl.append("xyz");
  op->setxattr("A", bl);
  op->setxattr("B", bl);

  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  ms.load_metadata(&obj);
  std::cout << "load metadata ok" << std::endl;
  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage.stat_mail(obj.get_oid(), &size, &save_date);

  EXPECT_EQ(buffer_length, size);
  EXPECT_EQ(0, ret_storage);
  EXPECT_EQ(0, ret_stat);
  EXPECT_EQ(5, (int)obj.get_completion_op_map()->size());
  EXPECT_EQ(2, (int)obj.get_metadata()->size());
  std::cout << " load with null" << std::endl;
  int i = ms.load_metadata(nullptr);
  EXPECT_EQ(-1, i);
  // obj->get_metadata()->size == 2
  i = ms.load_metadata(&obj);
  EXPECT_EQ(0, i);

  // remove it
  int ret_remove = storage.delete_mail(obj.get_oid());
  EXPECT_EQ(0, ret_remove);

  // tear down
  cluster.deinit();
}

TEST(librmb, AttributeVersions) {
  uint64_t max_size = 3;
  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);

  obj.set_oid("test_oid2");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosMetadataStorageDefault ms(&storage.get_io_ctx());

  ceph::bufferlist bl;
  bl.append("xyz");
  op->setxattr("A", bl);

  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);
  EXPECT_EQ(ret_storage, 0);
  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // stat the object
  uint64_t size;
  time_t save_date;
  int ret_stat = storage.stat_mail(obj.get_oid(), &size, &save_date);
  EXPECT_EQ(ret_stat, 0);
  uint64_t version = storage.get_io_ctx().get_last_version();

  // update metadata
  librmb::RadosMetadata metadata(librmb::RBOX_METADATA_OLDV1_KEYWORDS, "abc");
  ms.set_metadata(&obj, metadata);

  uint64_t version_after_xattr_update = storage.get_io_ctx().get_last_version();
  EXPECT_NE(version, version_after_xattr_update);

  std::map<std::string, librados::bufferlist> map;
  librados::bufferlist omap_bl;
  omap_bl.append("xxx");
  map.insert(std::pair<std::string, librados::bufferlist>(obj.get_oid(), omap_bl));
  storage.get_io_ctx().omap_set(obj.get_oid(), map);

  uint64_t version_after_omap_set = storage.get_io_ctx().get_last_version();
  EXPECT_NE(version_after_xattr_update, version_after_omap_set);

  // remove it
  storage.delete_mail(obj.get_oid());
  // tear down
  cluster.deinit();
}

// standard call order for metadata updates
// 1. save_metadata
// 2. set_metadata (update uid)
TEST(librmb, json_ima) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosDovecotCephCfgImpl cfg(&storage.get_io_ctx());

  // cfg.update_updatable_attributes("");
  librmb::RadosMetadataStorageIma ms(&storage.get_io_ctx(), &cfg);

  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  obj.set_oid("test_ima");
  unsigned int flags = 0x18;
  long recv_time = 12345677;
  // all attributes are not updateable.
  librmb::RadosMetadata attr(librmb::RBOX_METADATA_GUID, "guid");
  librmb::RadosMetadata attr2(librmb::RBOX_METADATA_OLDV1_FLAGS, flags);
  librmb::RadosMetadata attr3(librmb::RBOX_METADATA_RECEIVED_TIME, recv_time);
  librmb::RadosMetadata attr4(librmb::RBOX_METADATA_VERSION, "0.1");

  obj.add_metadata(attr);
  obj.add_metadata(attr2);
  obj.add_metadata(attr3);
  obj.add_metadata(attr4);

  ms.save_metadata(op, &obj);
  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);
  EXPECT_EQ(ret_storage, 0);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // check
  std::map<std::string, ceph::bufferlist> attr_list;
  storage.get_io_ctx().getxattrs(obj.get_oid(), attr_list);
  EXPECT_EQ(1, attr_list.size());

  unsigned int uid = 10;
  librmb::RadosMetadata attr_uid(librmb::RBOX_METADATA_MAIL_UID, uid);

  ms.set_metadata(&obj, attr_uid);

  // check again
  attr_list.clear();
  storage.get_io_ctx().getxattrs(obj.get_oid(), attr_list);
  EXPECT_EQ(1, attr_list.size());

  storage.delete_mail(&obj);
  // tear down
  cluster.deinit();
}
// standard call order for metadata updates
// 0. pre-condition: setting flags as updateable
// 1. save_metadata
// 2. set_metadata (update uid)
TEST(librmb, json_ima_2) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosDovecotCephCfgImpl cfg(&storage.get_io_ctx());

  cfg.set_update_attributes("true");
  cfg.update_updatable_attributes("F");
  librmb::RadosMetadataStorageIma ms(&storage.get_io_ctx(), &cfg);

  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  obj.set_oid("test_ima");
  unsigned int flags = 0x18;
  long recv_time = 12345677;
  // all attributes are not updateable.
  librmb::RadosMetadata attr(librmb::RBOX_METADATA_GUID, "guid");
  librmb::RadosMetadata attr2(librmb::RBOX_METADATA_OLDV1_FLAGS, flags);
  librmb::RadosMetadata attr3(librmb::RBOX_METADATA_RECEIVED_TIME, recv_time);
  librmb::RadosMetadata attr4(librmb::RBOX_METADATA_VERSION, "0.1");

  obj.add_metadata(attr);
  obj.add_metadata(attr2);
  obj.add_metadata(attr3);
  obj.add_metadata(attr4);

  ms.save_metadata(op, &obj);
  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);
  EXPECT_EQ(ret_storage, 0);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // check there should be ima and F (Flags)
  std::map<std::string, ceph::bufferlist> attr_list;
  storage.get_io_ctx().getxattrs(obj.get_oid(), attr_list);
  EXPECT_EQ(2, attr_list.size());

  unsigned int uid = 10;
  librmb::RadosMetadata attr_uid(librmb::RBOX_METADATA_MAIL_UID, uid);

  ms.set_metadata(&obj, attr_uid);

  // check again
  attr_list.clear();
  storage.get_io_ctx().getxattrs(obj.get_oid(), attr_list);
  EXPECT_EQ(2, attr_list.size());

  storage.delete_mail(&obj);
  // tear down
  cluster.deinit();
}

// standard call order for metadata updates
// 0. pre-condition: setting flags as updateable
// 1. save_metadata with keywords
// 2. set_metadata (update uid)
TEST(librmb, json_ima_3) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosDovecotCephCfgImpl cfg(&storage.get_io_ctx());
  cfg.set_update_attributes("true");
  cfg.update_updatable_attributes("FK");
  librmb::RadosMetadataStorageIma ms(&storage.get_io_ctx(), &cfg);

  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  obj.set_oid("test_ima");
  unsigned int flags = 0x18;
  long recv_time = 12345677;
  // all attributes are not updateable.
  librmb::RadosMetadata attr(librmb::RBOX_METADATA_GUID, "guid");
  librmb::RadosMetadata attr2(librmb::RBOX_METADATA_OLDV1_FLAGS, flags);
  librmb::RadosMetadata attr3(librmb::RBOX_METADATA_RECEIVED_TIME, recv_time);
  librmb::RadosMetadata attr4(librmb::RBOX_METADATA_VERSION, "0.1");

  obj.add_metadata(attr);
  obj.add_metadata(attr2);
  obj.add_metadata(attr3);
  obj.add_metadata(attr4);

  for (int i = 0; i < 10; i++) {
    std::string keyword = std::to_string(i);
    std::string ext_key = "k_" + keyword;
    librmb::RadosMetadata ext_metadata(ext_key, keyword);
    obj.add_extended_metadata(ext_metadata);
  }

  ms.save_metadata(op, &obj);
  int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);
  EXPECT_EQ(ret_storage, 0);

  // wait for op to finish.
  storage.wait_for_write_operations_complete(obj.get_completion_op_map());

  // check there should be ima and F (Flags)
  std::map<std::string, ceph::bufferlist> attr_list;
  storage.get_io_ctx().getxattrs(obj.get_oid(), attr_list);
  EXPECT_EQ(2, attr_list.size());

  unsigned int uid = 10;
  librmb::RadosMetadata attr_uid(librmb::RBOX_METADATA_MAIL_UID, uid);

  ms.set_metadata(&obj, attr_uid);

  // check again
  attr_list.clear();
  storage.get_io_ctx().getxattrs(obj.get_oid(), attr_list);
  EXPECT_EQ(2, attr_list.size());

  obj.get_metadata()->clear();
  obj.get_extended_metadata()->clear();
  std::cout << "loading metatadata" << std::endl;
  ms.load_metadata(&obj);

  EXPECT_EQ(10, obj.get_extended_metadata()->size());

  storage.delete_mail(&obj);
  // tear down
  cluster.deinit();
}
TEST(librmb, test_default_metadata_load_attributes) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librados::ObjectWriteOperation *op = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosDovecotCephCfgImpl cfg(&storage.get_io_ctx());
  cfg.set_update_attributes("true");
  cfg.update_updatable_attributes("FK");
  librmb::RadosMetadataStorageDefault ms(&storage.get_io_ctx());

  librmb::RadosMailObject obj;
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  obj.set_oid("test_ima");
  unsigned int flags = 0x18;
  long recv_time = 12345677;
  // all attributes are not updateable.
  librmb::RadosMetadata attr(librmb::RBOX_METADATA_GUID, "guid");
  librmb::RadosMetadata attr2(librmb::RBOX_METADATA_OLDV1_FLAGS, flags);
  librmb::RadosMetadata attr3(librmb::RBOX_METADATA_RECEIVED_TIME, recv_time);
  librmb::RadosMetadata attr4(librmb::RBOX_METADATA_VERSION, "0.1");

  obj.add_metadata(attr);
  obj.add_metadata(attr2);
  obj.add_metadata(attr3);
  obj.add_metadata(attr4);

  for (int i = 0; i < 10; i++) {
    std::string keyword = std::to_string(i);
    std::string ext_key = "k_" + keyword;
    librmb::RadosMetadata ext_metadata(ext_key, keyword);
    obj.add_extended_metadata(ext_metadata);
   }

   ms.save_metadata(op, &obj);
   int ret_storage = storage.split_buffer_and_exec_op(&obj, op, max_size);
   EXPECT_EQ(ret_storage, 0);

   // wait for op to finish.
   storage.wait_for_write_operations_complete(obj.get_completion_op_map());

   librmb::RadosMailObject obj2;
   obj2.set_oid("test_ima");

   int a = ms.load_metadata(&obj2);
   EXPECT_EQ(true, a >= 0);

   storage.delete_mail(&obj);
   // tear down
   cluster.deinit();
}

TEST(librmb, test_default_metadata_load_attributes_obj_no_longer_exist) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosDovecotCephCfgImpl cfg(&storage.get_io_ctx());
  cfg.set_update_attributes("true");
  cfg.update_updatable_attributes("FK");
  librmb::RadosMetadataStorageDefault ms(&storage.get_io_ctx());

  librmb::RadosMailObject obj2;
  obj2.set_oid("test_ima1");

  int a = ms.load_metadata(&obj2);
  EXPECT_EQ(-2, a);

  // tear down
  cluster.deinit();
}

TEST(librmb, test_default_metadata_load_attributes_obj_no_longer_exist_ima) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosDovecotCephCfgImpl cfg(&storage.get_io_ctx());
  cfg.set_update_attributes("true");
  cfg.update_updatable_attributes("FK");
  librmb::RadosMetadataStorageIma ms(&storage.get_io_ctx(), &cfg);

  librmb::RadosMailObject obj2;
  obj2.set_oid("test_ima1");

  int a = ms.load_metadata(&obj2);
  EXPECT_EQ(-2, a);

  // tear down
  cluster.deinit();
}

TEST(librmb, increment_add_to_non_existing_key) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("dictionary");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  std::string key = "my-key";

  librmb::RadosMailObject obj2;
  obj2.set_oid("myobject");

  ceph::bufferlist mail_buf;
  storage.save_mail(obj2.get_oid(), mail_buf);

  double val = 10;  // value to add
  int ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), obj2.get_oid(), key, val);

  // get the value!
  std::set<std::string> keys;
  std::map<std::string, ceph::bufferlist> omap;
  keys.insert(key);

  ASSERT_EQ(0, storage.get_io_ctx().omap_get_vals_by_keys(obj2.get_oid(), keys, &omap));

  std::map<std::string, ceph::bufferlist>::iterator it = omap.find(key);
  ASSERT_NE(omap.end(), it);

  ceph::bufferlist bl = (*it).second;
  EXPECT_EQ(bl.to_str(), "10");
  storage.delete_mail(&obj2);
  // tear down
  cluster.deinit();
}

TEST(librmb, increment_add_to_existing_key) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("dictionary");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  std::string key = "my-key";

  librmb::RadosMailObject obj2;
  obj2.set_oid("myobject");

  ceph::bufferlist mail_buf;
  storage.save_mail(obj2.get_oid(), mail_buf);
  double val = 10;  // value to add
  int ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), obj2.get_oid(), key, val);
  ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), obj2.get_oid(), key, val);
  // get the value!
  std::set<std::string> keys;
  std::map<std::string, ceph::bufferlist> omap;
  keys.insert(key);

  ASSERT_EQ(0, storage.get_io_ctx().omap_get_vals_by_keys(obj2.get_oid(), keys, &omap));

  std::map<std::string, ceph::bufferlist>::iterator it = omap.find(key);
  ASSERT_NE(omap.end(), it);

  ceph::bufferlist bl = (*it).second;

  EXPECT_EQ(bl.to_str(), "20");
  storage.delete_mail(&obj2);
  // tear down
  cluster.deinit();
}

TEST(librmb, increment_sub_from_existing_key) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("dictionary");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  std::string key = "my-key";

  librmb::RadosMailObject obj2;
  obj2.set_oid("myobject");

  bool b = true;
  ceph::bufferlist mail_buf;
  storage.save_mail(obj2.get_oid(), mail_buf);

  double val = 10;  // value to add
  int ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), obj2.get_oid(), key, val);
  double sub_val = 5;  // value to add
  ret = librmb::RadosUtils::osd_sub(&storage.get_io_ctx(), obj2.get_oid(), key, sub_val);
  // get the value!
  std::set<std::string> keys;
  std::map<std::string, ceph::bufferlist> omap;
  keys.insert(key);

  ASSERT_EQ(0, storage.get_io_ctx().omap_get_vals_by_keys(obj2.get_oid(), keys, &omap));

  std::map<std::string, ceph::bufferlist>::iterator it = omap.find(key);
  ASSERT_NE(omap.end(), it);

  ceph::bufferlist bl = (*it).second;

  EXPECT_EQ(bl.to_str(), "5");
  storage.delete_mail(&obj2);
  // tear down
  cluster.deinit();
}

TEST(librmb, mock_obj) {}
int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
