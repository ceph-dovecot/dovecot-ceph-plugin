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

#include <algorithm>
#include "../../librmb/rados-cluster-impl.h"
#include "../../librmb/rados-storage-impl.h"
#include "mock_test.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "../../librmb/rados-metadata-storage-default.h"
#include "../../librmb/rados-metadata-storage-ima.h"
#include "../../librmb/rados-dovecot-ceph-cfg-impl.h"
#include "../../librmb/rados-util.h"
#include "../../librmb/tools/rmb/rmb-commands.h"
#include "../../librmb/rados-save-log.h"

using ::testing::AtLeast;
using ::testing::Return;

/**
 * Test object split operation
 *
 */
// TEST(librmb, split_write_operation) {
//   uint64_t max_size = 3;
//   librmb::RadosMail obj;
//   librados::bufferlist buffer;
//   obj.set_mail_buffer(&buffer);
//   obj.get_mail_buffer()->append("abcdefghijklmn");
//   std::cout << "jsjjsjssjs" << std::endl;
//   size_t buffer_length = obj.get_mail_buffer()->length();
//   std::cout << "lenght" << std::endl;

//   obj.set_mail_size(buffer_length);
//   obj.set_oid("test_oid");
//   librados::IoCtx io_ctx;

//   librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
//   librmb::RadosClusterImpl cluster;
//   librmb::RadosStorageImpl storage(&cluster);

//   std::string pool_name("test");
//   std::string ns("t");
//   std::cout << "open" << std::endl;
//   int open_connection = storage.open_connection(pool_name);
//   std::cout << "pok" << std::endl;
//   storage.set_namespace(ns);
//   EXPECT_EQ(0, open_connection);
//   std::cout << "accccc" << std::endl;
//   // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);
//   std::cout << "lsdlslslss" << std::endl;

//   // stat the object
//   uint64_t size;
//   time_t save_date;
//   int ret_stat = storage.stat_mail(*obj.get_oid(), &size, &save_date);
//  std::cout << "storage.stat_mail" << std::endl;
//   // remove it
//   int ret_remove = storage.delete_mail(&obj);

//   // tear down
//   cluster.deinit();

//   EXPECT_EQ(buffer_length, size);
//   // EXPECT_EQ(0, ret_storage);
//   EXPECT_EQ(0, ret_stat);
//   EXPECT_EQ(0, ret_remove);
//   EXPECT_EQ(0, (int)obj.get_num_active_op());
  
// }
/**
 * Test object split operation
 *
 */
// TEST(librmb1, split_write_operation_1) {
//   librmb::RadosMail obj;
//   librados::bufferlist buffer;
//   obj.set_mail_buffer(&buffer);
//   obj.get_mail_buffer()->append("HALLO_WELT_");
//   size_t buffer_length = obj.get_mail_buffer()->length();
//   obj.set_mail_size(buffer_length);
//   uint64_t max_size = buffer_length;

//   obj.set_oid("test_oid");
//   librados::IoCtx io_ctx;

//   librados::ObjectWriteOperation op;  //= new librados::ObjectWriteOperation();
//   librmb::RadosClusterImpl cluster;
//   librmb::RadosStorageImpl storage(&cluster);

//   std::string pool_name("test");
//   std::string ns("t");

//   int open_connection = storage.open_connection(pool_name);
//   storage.set_namespace(ns);
//   EXPECT_EQ(0, open_connection);

//   int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);

//   // stat the object
//   uint64_t size;
//   time_t save_date;
//   int ret_stat = storage.stat_mail(*obj.get_oid(), &size, &save_date);

//   // remove it
//   int ret_remove = storage.delete_mail(*obj.get_oid());

//   // tear down.
//   cluster.deinit();

//   EXPECT_EQ(buffer_length, size);
//   EXPECT_EQ(0, ret_storage);
//   EXPECT_EQ(0, ret_stat);
//   EXPECT_EQ(0, ret_remove);
//   EXPECT_EQ(0, (int)obj.get_num_active_op());
// }
/**
 * Test Rados Metadata type conversion
 *
 */
TEST(librmb1, convert_types) {
  std::string value = "4441c5339f4c9d59523000009c60b9f7";
  librmb::RadosMetadata attr(librmb::RBOX_METADATA_GUID, value);

  EXPECT_EQ(attr.key, "G");
  EXPECT_STREQ(attr.bl.c_str(), "4441c5339f4c9d59523000009c60b9f7");
  time_t t = 1503488583;

  attr.key = "";
  attr.bl.clear();
  librmb::RadosMetadata attr2(librmb::RBOX_METADATA_RECEIVED_TIME, t);

  EXPECT_EQ(attr2.key, "R");
  EXPECT_STREQ(attr2.bl.c_str(), "1503488583");

  time_t recv_date;
  attr2.convert(attr2.bl.c_str(), &recv_date);
  EXPECT_EQ(t, recv_date);

  size_t st = 100;
  librmb::RadosMetadata attr4(librmb::RBOX_METADATA_VIRTUAL_SIZE, st);
  EXPECT_EQ(attr4.key, "V");
  EXPECT_STREQ(attr4.bl.c_str(), "100");

  attr4.key = "";
  attr4.bl.clear();
}
/**
 * Test Storage read_mail
 *
 */
TEST(librmb1, read_mail) {
  librmb::RadosMail obj;
  librados::bufferlist buffer;
  obj.set_mail_buffer(&buffer);
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);
  uint64_t max_size = buffer_length;

  obj.set_oid("test_oid");
  librados::IoCtx io_ctx;

  // librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);

  // wait for op to finish.
  // storage.wait_for_write_operations_complete(obj.get_completion(), obj.get_write_operation());

  // stat the object
  // uint64_t size;
  // time_t save_date;
  // int ret_stat = storage.stat_mail(*obj.get_oid(), &size, &save_date);
  uint64_t size;
  librados::bufferlist bl;
  int copy_mail_ret = storage.read_mail(*obj.get_oid())->get_ret_read_op();
  char *buff = new char[copy_mail_ret + 1];
  memset(buff, 1, size + 1);
  memcpy(buff, bl.to_str().c_str(), copy_mail_ret + 1);
  EXPECT_EQ(buff[copy_mail_ret], '\0');

  // remove it
  int ret_remove = storage.delete_mail(*obj.get_oid());

  // tear down
  cluster.deinit();
  // EXPECT_EQ(ret_storage, 0);
  // EXPECT_EQ(ret_stat, 0);
  EXPECT_EQ(ret_remove, 0);
  EXPECT_EQ(copy_mail_ret, 14);
  EXPECT_EQ(buff[0], 'a');
  EXPECT_EQ(buff[1], 'b');
  EXPECT_EQ(buff[2], 'c');
  EXPECT_EQ(buff[3], 'd');

  delete[] buff;
}
/**
 * Test Load Metadata
 *
 */
// TEST(librmb, load_metadata) {
//   uint64_t max_size = 3;
//   librmb::RadosMail obj;
//   librados::bufferlist buffer;
//   obj.set_mail_buffer(&buffer);
//   obj.get_mail_buffer()->append("abcdefghijklmn");
//   size_t buffer_length = obj.get_mail_buffer()->length();
//   obj.set_mail_size(buffer_length);

//   obj.set_oid("test_oid");
//   obj.get_io_
//   std::cout<<"GET-oid"<<obj.get_oid();
//   librados::IoCtx io_ctx;

//   librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
//   librmb::RadosClusterImpl cluster;
//   librmb::RadosStorageImpl storage(&cluster);
//   std::string pool_name("test");
//   std::string ns("t");

//   int open_connection = storage.open_connection(pool_name);
//   storage.set_namespace(ns);
//   EXPECT_EQ(0, open_connection);

//   librmb::RadosMetadataStorageDefault ms(&storage.get_io_ctx());

//   // ceph::bufferlist bl;
//   // bl.append("xyz\0");
//   // op.setxattr("A", bl);
//   // op.setxattr("B", bl);

//   // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);

//   ms.load_metadata(&obj);
//   std::cout << "load metadata ok" << std::endl;
//   // stat the object
//   uint64_t size;
//   time_t save_date;
//   // int ret_stat = storage.stat_mail(*obj.get_oid(), &size, &save_date);
//   std::cout<<"GET-BUFFER"<<obj.get_mail_size();
//   EXPECT_EQ(buffer_length, size);
//   // EXPECT_EQ(0, ret_storage);
//   // EXPECT_EQ(0, ret_stat);
//   EXPECT_EQ(0, (int)obj.get_num_active_op());
//   // EXPECT_EQ(2, (int)obj.get_metadata()->size());
//   std::cout << " load with null" << std::endl;
//   int i = ms.load_metadata(nullptr);
//   EXPECT_EQ(-1, i);
//   // obj->get_metadata()->size == 2
//   i = ms.load_metadata(&obj);
//   EXPECT_EQ(0, i);

//   // remove it
//   int ret_remove = storage.delete_mail(*obj.get_oid());
//   EXPECT_EQ(0, ret_remove);

//   // tear down
//   cluster.deinit();
//   delete &size;
// }
/**
 * rados object version behavior
 *
 */
TEST(librmb, AttributeVersions) {
  uint64_t max_size = 3;
  librmb::RadosMail obj;
  librados::bufferlist buffer;
  obj.set_mail_buffer(&buffer);
  obj.get_mail_buffer()->append("abcdefghijklmn");
  size_t buffer_length = obj.get_mail_buffer()->length();
  obj.set_mail_size(buffer_length);

  obj.set_oid("test_oid2");
  librados::IoCtx io_ctx;

  librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("test");
  std::string ns("t");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  librmb::RadosMetadataStorageDefault ms(&storage.get_io_ctx());

  ceph::bufferlist bl;
  bl.append("xyz\0");
  op.setxattr("A", bl);

  // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);
  // EXPECT_EQ(ret_storage, 0);
  
  // stat the object
  // uint64_t size;
  // time_t save_date;
  // int ret_stat = storage.stat_mail(*obj.get_oid(), &size, &save_date);
  // EXPECT_EQ(ret_stat, 0);
  uint64_t version = storage.get_io_ctx().get_last_version();

  // update metadata
  librmb::RadosMetadata metadata(librmb::RBOX_METADATA_OLDV1_KEYWORDS, "abc");
  ms.set_metadata(&obj, metadata);

  uint64_t version_after_xattr_update = storage.get_io_ctx().get_last_version();
  EXPECT_NE(version, version_after_xattr_update);

  std::map<std::string, librados::bufferlist> map;
  librados::bufferlist omap_bl;
  omap_bl.append("xxx");
  map.insert(std::pair<std::string, librados::bufferlist>(*obj.get_oid(), omap_bl));
  storage.get_io_ctx().omap_set(*obj.get_oid(), map);
  uint64_t version_after_omap_set = storage.get_io_ctx().get_last_version();
  EXPECT_NE(version_after_xattr_update, version_after_omap_set);

  // remove it
  storage.delete_mail(*obj.get_oid());
  // tear down
  cluster.deinit();
}

// standard call order for metadata updates
// 1. save_metadata
// 2. set_metadata (update uid)
TEST(librmb, json_ima) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
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

  librmb::RadosMail obj;
  librados::bufferlist buffer;
  obj.set_mail_buffer(&buffer);
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

  ms.save_metadata(&op, &obj);
  // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);
  // EXPECT_EQ(ret_storage, 0);

  // wait for op to finish.
  // storage.wait_for_write_operations_complete(obj.get_completion(), obj.get_write_operation());

  // check
  std::map<std::string, ceph::bufferlist> attr_list;
  // storage.get_io_ctx().getxattrs(*obj.get_oid(), attr_list);
  // EXPECT_EQ(1, attr_list.size());

  unsigned int uid = 10;
  librmb::RadosMetadata attr_uid(librmb::RBOX_METADATA_MAIL_UID, uid);

  ms.set_metadata(&obj, attr_uid);

  // check again
  attr_list.clear();
  storage.get_io_ctx().getxattrs(*obj.get_oid(), attr_list);
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

  librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
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

  librmb::RadosMail obj;
  librados::bufferlist buffer;
  obj.set_mail_buffer(&buffer);
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

  ms.save_metadata(&op, &obj);
  // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);
  // EXPECT_EQ(ret_storage, 0);

  // wait for op to finish.
  // storage.wait_for_write_operations_complete(obj.get_completion(), obj.get_write_operation());

  // check there should be ima and F (Flags)
  std::map<std::string, ceph::bufferlist> attr_list;
  // storage.get_io_ctx().getxattrs(*obj.get_oid(), attr_list);
  // EXPECT_EQ(2, attr_list.size());

  unsigned int uid = 10;
  librmb::RadosMetadata attr_uid(librmb::RBOX_METADATA_MAIL_UID, uid);

  ms.set_metadata(&obj, attr_uid);

  // check again
  attr_list.clear();
  storage.get_io_ctx().getxattrs(*obj.get_oid(), attr_list);
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

  librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
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

  librmb::RadosMail obj;
  librados::bufferlist buffer;
  obj.set_mail_buffer(&buffer);
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

  ms.save_metadata(&op, &obj);
  // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);
  // EXPECT_EQ(ret_storage, 0);

  // wait for op to finish.
  // storage.wait_for_write_operations_complete(obj.get_completion(), obj.get_write_operation());

  // check there should be ima and F (Flags)
  std::map<std::string, ceph::bufferlist> attr_list;
  // storage.get_io_ctx().getxattrs(*obj.get_oid(), attr_list);
  // EXPECT_EQ(2, attr_list.size());

  unsigned int uid = 10;
  librmb::RadosMetadata attr_uid(librmb::RBOX_METADATA_MAIL_UID, uid);

  ms.set_metadata(&obj, attr_uid);

  // check again
  attr_list.clear();
  storage.get_io_ctx().getxattrs(*obj.get_oid(), attr_list);
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
/**
 * Load metadata with default metadata reader
 */
TEST(librmb, test_default_metadata_load_attributes) {
  librados::IoCtx io_ctx;
  uint64_t max_size = 3;

  librados::ObjectWriteOperation op;  // = new librados::ObjectWriteOperation();
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

  librmb::RadosMail obj;
  librados::bufferlist buffer;
  obj.set_mail_buffer(&buffer);
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

  ms.save_metadata(&op, &obj);
  // int ret_storage = storage.split_buffer_and_exec_op(&obj, &op, max_size);
  // EXPECT_EQ(ret_storage, 0);

  // wait for op to finish.
  // storage.wait_for_write_operations_complete(obj.get_completion(), obj.get_write_operation());

  librmb::RadosMail obj2;
  obj2.set_oid("test_ima");

  int a = ms.load_metadata(&obj2);
  // EXPECT_EQ(true, a >= 0);

  storage.delete_mail(&obj);
  // tear down
  cluster.deinit();
}
/**
 * Test LoadMetadata default reader
 */
TEST(librmb, test_default_metadata_load_attributes_obj_no_longer_exist) {
  librados::IoCtx io_ctx;

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

  librmb::RadosMail obj2;
  obj2.set_oid("test_ima1");

  int a = ms.load_metadata(&obj2);
  EXPECT_EQ(-2, a);

  // tear down
  cluster.deinit();
}
/**
 * Test Metadata reader with ima reader
 */
TEST(librmb, test_default_metadata_load_attributes_obj_no_longer_exist_ima) {
  librados::IoCtx io_ctx;

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

  librmb::RadosMail obj2;
  obj2.set_oid("test_ima1");

  int a = ms.load_metadata(&obj2);
  EXPECT_EQ(-2, a);

  // tear down
  cluster.deinit();
}
/**
 * Test osd increment
 */
TEST(librmb, increment_add_to_non_existing_key) {
  librados::IoCtx io_ctx;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("dictionary");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  std::string key = "my-key";

  librmb::RadosMail obj2;
  obj2.set_oid("myobject");

  ceph::bufferlist mail_buf;
  storage.save_mail(*obj2.get_oid(), mail_buf);

  long val = 10;  // value to add
  int ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), *obj2.get_oid(), key, val);
  ASSERT_EQ(0, ret);
  // get the value!
  std::set<std::string> keys;
  std::map<std::string, ceph::bufferlist> omap;
  keys.insert(key);

  ASSERT_EQ(0, storage.get_io_ctx().omap_get_vals_by_keys(*obj2.get_oid(), keys, &omap));

  std::map<std::string, ceph::bufferlist>::iterator it = omap.find(key);
  ASSERT_NE(omap.end(), it);

  ceph::bufferlist bl = (*it).second;
  EXPECT_EQ(bl.to_str(), "10");
  storage.delete_mail(&obj2);
  // tear down
  cluster.deinit();
}
/**
 * Test osd increment
 */
TEST(librmb, increment_add_to_non_existing_object) {
  librados::IoCtx io_ctx;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("dictionary");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  std::string key = "my-key";

  librmb::RadosMail obj2;
  obj2.set_oid("myobject");

  long val = 10;  // value to add
  int ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), *obj2.get_oid(), key, val);
  ASSERT_EQ(0, ret);
  ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), *obj2.get_oid(), key, val);
  ASSERT_EQ(0, ret);
  // get the value!
  std::set<std::string> keys;
  std::map<std::string, ceph::bufferlist> omap;
  keys.insert(key);

  ASSERT_EQ(0, storage.get_io_ctx().omap_get_vals_by_keys(*obj2.get_oid(), keys, &omap));

  std::map<std::string, ceph::bufferlist>::iterator it = omap.find(key);
  ASSERT_NE(omap.end(), it);

  ceph::bufferlist bl = (*it).second;

  EXPECT_EQ(bl.to_str(), "20");
  storage.delete_mail(&obj2);
  // tear down
  cluster.deinit();
}
/**
 * Test osd increment
 */
TEST(librmb, increment_add_to_existing_key) {
  librados::IoCtx io_ctx;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("dictionary");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  std::string key = "my-key";

  librmb::RadosMail obj2;
  obj2.set_oid("myobject");

  ceph::bufferlist mail_buf;
  storage.save_mail(*obj2.get_oid(), mail_buf);
  long val = 10;  // value to add
  int ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), *obj2.get_oid(), key, val);
  ASSERT_EQ(0, ret);
  ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), *obj2.get_oid(), key, val);
  ASSERT_EQ(0, ret);
  // get the value!
  std::set<std::string> keys;
  std::map<std::string, ceph::bufferlist> omap;
  keys.insert(key);

  ASSERT_EQ(0, storage.get_io_ctx().omap_get_vals_by_keys(*obj2.get_oid(), keys, &omap));

  std::map<std::string, ceph::bufferlist>::iterator it = omap.find(key);
  ASSERT_NE(omap.end(), it);

  ceph::bufferlist bl = (*it).second;

  EXPECT_EQ(bl.to_str(), "20");
  storage.delete_mail(&obj2);
  // tear down
  cluster.deinit();
}

/**
 * Test osd decrement
 */
TEST(librmb, increment_sub_from_existing_key) {
  librados::IoCtx io_ctx;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("dictionary");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  storage.set_namespace(ns);
  EXPECT_EQ(0, open_connection);

  std::string key = "my-key";

  librmb::RadosMail obj2;
  obj2.set_oid("myobject");

  ceph::bufferlist mail_buf;
  storage.save_mail(*obj2.get_oid(), mail_buf);

  long val = 10;  // value to add
  int ret = librmb::RadosUtils::osd_add(&storage.get_io_ctx(), *obj2.get_oid(), key, val);
  ASSERT_EQ(0, ret);
  long sub_val = 5;  // value to add
  ret = librmb::RadosUtils::osd_sub(&storage.get_io_ctx(), *obj2.get_oid(), key, sub_val);
  // get the value!
  ASSERT_EQ(0, ret);
  std::set<std::string> keys;
  std::map<std::string, ceph::bufferlist> omap;
  keys.insert(key);

  ASSERT_EQ(0, storage.get_io_ctx().omap_get_vals_by_keys(*obj2.get_oid(), keys, &omap));

  std::map<std::string, ceph::bufferlist>::iterator it = omap.find(key);
  ASSERT_NE(omap.end(), it);

  ceph::bufferlist bl = (*it).second;

  EXPECT_EQ(bl.to_str(), "5");
  storage.delete_mail(&obj2);
  // tear down
  cluster.deinit();
}
/**
 * RmbCommands load objects
 */
TEST(librmb, rmb_load_objects) {
  librados::IoCtx io_ctx;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);

  EXPECT_EQ(0, open_connection);
  librmb::RadosCephConfig ceph_cfg(&storage.get_io_ctx());
  EXPECT_EQ(0, ceph_cfg.save_cfg());

  std::map<std::string, std::string> opts;
  opts["pool"] = pool_name;
  opts["namespace"] = ns;
  opts["print_cfg"] = "true";
  opts["cfg_obj"] = ceph_cfg.get_cfg_object_name();

  librmb::RmbCommands rmb_commands(&storage, &cluster, &opts);

  /* update config
  rmb_commands.configuration(false, ceph_cfg);
  */
  // load metadata info
  std::string uid;
  librmb::RadosStorageMetadataModule *ms = rmb_commands.init_metadata_storage_module(ceph_cfg, &uid);
  EXPECT_NE(nullptr, ms);

  storage.set_namespace(ns);
  librmb::RadosMail obj2;
  obj2.set_oid("myobject");

  ceph::bufferlist mail_buf;
  storage.save_mail(*obj2.get_oid(), mail_buf);

  std::list<librmb::RadosMail *> mail_objects;
  std::string sort_string = "uid";

  EXPECT_EQ(0, rmb_commands.load_objects(ms, mail_objects, sort_string,true));
  EXPECT_EQ(1, mail_objects.size());
  
  for (std::list<librmb::RadosMail *>::iterator it = mail_objects.begin(); it != mail_objects.end(); ++it) {
    librmb::RadosMail *obj = *it;
    delete obj;
  }

  storage.delete_mail(&obj2);
  storage.delete_mail(ceph_cfg.get_cfg_object_name());
  delete ms;
  delete &mail_objects;
  // tear down
  cluster.deinit();
}
/**
 * Test RmbCommands load objects
 */
TEST(librmb, rmb_load_objects_valid_metadata) {
  librados::IoCtx io_ctx;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);

  EXPECT_EQ(0, open_connection);
  librmb::RadosCephConfig ceph_cfg(&storage.get_io_ctx());
  EXPECT_EQ(0, ceph_cfg.save_cfg());

  std::map<std::string, std::string> opts;
  opts["pool"] = pool_name;
  opts["namespace"] = ns;
  opts["print_cfg"] = "true";
  opts["cfg_obj"] = ceph_cfg.get_cfg_object_name();

  librmb::RmbCommands rmb_commands(&storage, &cluster, &opts);

  // load metadata info
  std::string uid;
  librmb::RadosStorageMetadataModule *ms = rmb_commands.init_metadata_storage_module(ceph_cfg, &uid);
  EXPECT_NE(nullptr, ms);

  storage.set_namespace(ns);

  librmb::RadosMail obj2;
  librados::bufferlist *buffer = new librados::bufferlist();
  obj2.set_mail_buffer(buffer);
  obj2.set_oid("myobject_valid");
  obj2.get_mail_buffer()->append("hallo_welt");  // make sure obj is not empty.
  obj2.set_mail_size(obj2.get_mail_buffer()->length());
  librados::ObjectWriteOperation write_op;  // = new librados::ObjectWriteOperation();
  {
    std::string key = "M";
    std::string val = "8eed840764b05359f12718004d2485ee";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "I";
    std::string val = "v0.1";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "G";
    std::string val = "8eed840764b05359f12718004d2485ee";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "R";
    std::string val = "1234567";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "S";
    std::string val = "1234561117";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "P";
    std::string val = "1";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "O";
    std::string val = "0";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "Z";
    std::string val = "200";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "V";
    std::string val = "250";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "U";
    std::string val = "1";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "A";
    std::string val = "";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "F";
    std::string val = "01";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "B";
    std::string val = "DRAFTS";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  // convert metadata to xattr. and add to write_op
  ms->save_metadata(&write_op, &obj2);
  // save complete mail.
  EXPECT_EQ(true, storage.save_mail(&write_op, &obj2));
  std::list<librmb::RadosMail *> list;
  list.push_back(&obj2);

  EXPECT_EQ(true, !storage.wait_for_rados_operations(list));

  std::list<librmb::RadosMail *> mail_objects;
  std::string sort_string = "uid";

  EXPECT_EQ(0, rmb_commands.load_objects(ms, mail_objects, sort_string));
  // there needs to be one mail
  EXPECT_EQ(1, mail_objects.size());

  storage.delete_mail(&obj2);
  storage.delete_mail(ceph_cfg.get_cfg_object_name());
  for (std::list<librmb::RadosMail *>::iterator it = mail_objects.begin(); it != mail_objects.end(); ++it) {
    librmb::RadosMail *obj = *it;
    storage.delete_mail(obj);
    delete obj;
  }

  delete ms;

  mail_objects.clear();
  // tear down
  cluster.deinit();
}
/**
 * Test RmbCommands load objects
 */
TEST(librmb, rmb_load_objects_invalid_metadata) {
  librados::IoCtx io_ctx;

  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);

  EXPECT_EQ(0, open_connection);
  librmb::RadosCephConfig ceph_cfg(&storage.get_io_ctx());
  EXPECT_EQ(0, ceph_cfg.save_cfg());

  std::map<std::string, std::string> opts;
  opts["pool"] = pool_name;
  opts["namespace"] = ns;
  opts["print_cfg"] = "true";
  opts["cfg_obj"] = ceph_cfg.get_cfg_object_name();

  librmb::RmbCommands rmb_commands(&storage, &cluster, &opts);

  // load metadata info
  std::string uid;
  librmb::RadosStorageMetadataModule *ms = rmb_commands.init_metadata_storage_module(ceph_cfg, &uid);
  EXPECT_NE(nullptr, ms);

  storage.set_namespace(ns);

  librmb::RadosMail obj2;
  librados::bufferlist *buffer = new librados::bufferlist();
  obj2.set_mail_buffer(buffer);
  obj2.set_oid("myobject_invalid");
  obj2.get_mail_buffer()->append("hallo_welt");  // make sure obj is not empty.
  obj2.set_mail_size(obj2.get_mail_buffer()->length());
  librados::ObjectWriteOperation write_op;  // = new librados::ObjectWriteOperation();
  {
    std::string key = "M";
    std::string val = "8eed840764b05359f12718004d2485ee";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "I";
    std::string val = "v0.1";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "G";
    std::string val = "8eed840764b05359f12718004d2485ee";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "R";
    std::string val = "abnahsijsksisis";  // <-- This should be numeric
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "S";
    std::string val = "1234561117";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "P";
    std::string val = "1";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "O";
    std::string val = "0";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "Z";
    std::string val = "200";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "V";
    std::string val = "250";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "U";
    std::string val = "1";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "A";
    std::string val = "";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "F";
    std::string val = "01";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  {
    std::string key = "B";
    std::string val = "DRAFTS";
    librmb::RadosMetadata m(key, val);
    obj2.add_metadata(m);
  }
  // convert metadata to xattr. and add to write_op
  ms->save_metadata(&write_op, &obj2);
  // save complete mail.
  EXPECT_EQ(true, storage.save_mail(&write_op, &obj2));
  std::list<librmb::RadosMail *> list;
  list.push_back(&obj2);

  EXPECT_EQ(true, !storage.wait_for_rados_operations(list));

  std::list<librmb::RadosMail *> mail_objects;
  std::string sort_string = "uid";

  EXPECT_EQ(0, rmb_commands.load_objects(ms, mail_objects, sort_string));
  // no mail
  EXPECT_EQ(1, mail_objects.size());

  for (std::list<librmb::RadosMail *>::iterator it = mail_objects.begin(); it != mail_objects.end(); ++it) {
    librmb::RadosMail *obj = *it;
    delete obj;
  }

  storage.delete_mail(&obj2);
  storage.delete_mail(ceph_cfg.get_cfg_object_name());
  delete ms;
  mail_objects.clear();
  // tear down
  cluster.deinit();
}
/**
 * Test RmbCommands
 */
TEST(librmb, delete_objects_via_rmb_tool_and_save_log_file) {
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  EXPECT_EQ(0, open_connection);
  storage.set_namespace(ns);

  std::string test_file_name = "test1.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("abc", "t1", "rmb_tool_tests", "save"));
  EXPECT_EQ(true, log_file.close());
  librados::bufferlist bl;
  EXPECT_EQ(0, storage.save_mail("abc", bl));

  std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;
  EXPECT_EQ(1, librmb::RmbCommands::delete_with_save_log("test1.log", "ceph", "client.admin", &moved_items));
  std::remove(test_file_name.c_str());

  cluster.deinit();
}
/**
 * Test RmbCommands
 */
TEST(librmb, delete_objects_via_rmb_tool_and_save_log_file_file_not_found) {
  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  std::string test_file_name = "test1.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("abc", "t1", "rmb_tool_tests", "save"));
  EXPECT_EQ(true, log_file.close());
  std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;

  EXPECT_EQ(0, librmb::RmbCommands::delete_with_save_log("test1.log", "ceph", "client.admin", &moved_items));
  std::remove(test_file_name.c_str());
}
/**
 * Test RmbCommands
 */
TEST(librmb, delete_objects_via_rmb_tool_and_save_log_file_invalid_file) {
  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  std::string test_file_name = "test1.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("abc", "t1", "rmb_tool_tests", "save"));
  EXPECT_EQ(true, log_file.close());
  std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;

  EXPECT_EQ(-1, librmb::RmbCommands::delete_with_save_log("test12.log", "ceph", "client.admin", &moved_items));
  std::remove(test_file_name.c_str());
}
/**
 * Test RmbCommands
 */
TEST(librmb, delete_objects_via_rmb_tool_and_save_log_file_invalid_entry) {
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  EXPECT_EQ(0, open_connection);
  storage.set_namespace(ns);

  std::string test_file_name = "test1.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(
      librmb::RadosSaveLogEntry("abc2", "t1", "2,2,2rmb_tool_tests", "save"));  // -> stop processing (invalid entry)!
  log_file.append(librmb::RadosSaveLogEntry("abc2", "t1", "rmb_tool_tests", "save"));
  EXPECT_EQ(true, log_file.close());
  librados::bufferlist bl;

  EXPECT_EQ(0, storage.save_mail("abc2", bl));
  cluster.deinit();

  std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;

  EXPECT_EQ(0, librmb::RmbCommands::delete_with_save_log("test1.log", "ceph", "client.admin",
                                                         &moved_items));  // -> due to invalid entry in object list
  std::remove(test_file_name.c_str());

  open_connection = storage.open_connection(pool_name);
  EXPECT_EQ(0, open_connection);
  storage.set_namespace(ns);
  EXPECT_EQ(storage.delete_mail("abc2"), 0);  // check that save log processing does stop at invalid line!
  cluster.deinit();
}
/**
 * Test RmbCommands
 */
TEST(librmb, move_object_delete_with_save_log) {
  librmb::RadosClusterImpl cluster;
  librmb::RadosStorageImpl storage(&cluster);

  std::string pool_name("rmb_tool_tests");
  std::string ns("t1");

  int open_connection = storage.open_connection(pool_name);
  EXPECT_EQ(0, open_connection);
  storage.set_namespace(ns);

  std::string test_file_name = "test1.log";
  librmb::RadosSaveLog log_file(test_file_name);
  EXPECT_EQ(true, log_file.open());
  log_file.append(librmb::RadosSaveLogEntry("abc3", "t1", "rmb_tool_tests",
                                            "mv:t1:abc3:t1;M=123:B=INBOX:U=1:G=0246da2269ac1f5b3e1700009c60b9f7"));
  EXPECT_EQ(true, log_file.close());
  librados::bufferlist bl;
  EXPECT_EQ(0, storage.save_mail("abc3", bl));
  cluster.deinit();

  std::map<std::string, std::list<librmb::RadosSaveLogEntry>> moved_items;
  EXPECT_EQ(1, librmb::RmbCommands::delete_with_save_log("test1.log", "ceph", "client.admin", &moved_items));
  EXPECT_EQ(1, moved_items.size());
  std::list<librmb::RadosSaveLogEntry> list = moved_items["t1"];
  EXPECT_EQ(1, list.size());
  librmb::RadosSaveLogEntry entry = list.front();

  EXPECT_EQ(entry.src_oid, "abc3");
  EXPECT_EQ(entry.src_ns, "t1");
  EXPECT_EQ(entry.src_user, "t1");

  std::string key_guid(1, static_cast<char>(librmb::RBOX_METADATA_GUID));
  std::list<librmb::RadosMetadata>::iterator it_guid =
      std::find_if(entry.metadata.begin(), entry.metadata.end(),
                   [key_guid](librmb::RadosMetadata const &m) { return m.key == key_guid; });
  EXPECT_EQ("0246da2269ac1f5b3e1700009c60b9f7", (*it_guid).bl.to_str());

  std::remove(test_file_name.c_str());
  open_connection = storage.open_connection(pool_name);
  EXPECT_EQ(0, open_connection);
  storage.set_namespace(ns);
  EXPECT_EQ(storage.delete_mail("abc3"), 0);  // move does not delete the object
  cluster.deinit();
}
TEST(librmb, mock_obj) {}
int main(int argc, char **argv) {
  ::testing::InitGoogleMock(&argc, argv);
  return RUN_ALL_TESTS();
}
