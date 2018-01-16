/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */
#ifndef SRC_TESTS_MOCKS_MOCK_TEST_H_
#define SRC_TESTS_MOCKS_MOCK_TEST_H_

#include <map>
#include <string>

#include "../../librmb/rados-cluster.h"
#include "../../librmb/rados-dictionary.h"
#include "../../librmb/rados-dovecot-config.h"
#include "../../librmb/rados-storage.h"
#include "../../librmb/rados-dovecot-ceph-cfg.h"
#include "gmock/gmock.h"

namespace librmbtest {
using librmb::RadosStorage;
using librmb::RadosMailObject;
using librmb::RadosMetadata;

class RadosStorageMock : public RadosStorage {
 public:
  MOCK_METHOD0(get_io_ctx, librados::IoCtx &());
  MOCK_METHOD3(stat_mail, int(const std::string &oid, uint64_t *psize, time_t *pmtime));
  MOCK_METHOD1(set_namespace, void(const std::string &nspace));
  MOCK_METHOD0(get_namespace, std::string());
  MOCK_METHOD0(get_max_write_size, int());
  MOCK_METHOD0(get_max_write_size_bytes, int());

  MOCK_METHOD3(split_buffer_and_exec_op,
               int(RadosMailObject *current_object, librados::ObjectWriteOperation *write_op_xattr,
                   const uint64_t &max_write));

  MOCK_METHOD1(load_metadata, int(RadosMailObject *mail));
  MOCK_METHOD2(set_metadata, int(const std::string &oid, RadosMetadata &xattr));

  MOCK_METHOD1(delete_mail, int(RadosMailObject *mail));
  MOCK_METHOD1(delete_mail, int(const std::string &oid));
  MOCK_METHOD4(aio_operate, int(librados::IoCtx *io_ctx_, const std::string &oid, librados::AioCompletion *c,
                                librados::ObjectWriteOperation *op));
  MOCK_METHOD1(find_mails, librados::NObjectIterator(const RadosMetadata *attr));
  MOCK_METHOD1(open_connection, int(const std::string &poolname));
  MOCK_METHOD3(open_connection,
               int(const std::string &poolname, const std::string &clustername, const std::string &rados_username));

  MOCK_METHOD1(wait_for_write_operations_complete,
               bool(std::map<librados::AioCompletion *, librados::ObjectWriteOperation *> *completion_op_map));
  MOCK_METHOD1(wait_for_rados_operations, bool(const std::vector<librmb::RadosMailObject *> &object_list));

  MOCK_METHOD2(read_mail, int(const std::string &oid, librados::bufferlist *buffer));
  MOCK_METHOD2(update_metadata, bool(std::string &oid, std::list<RadosMetadata> &to_update));
  MOCK_METHOD6(move, bool(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                          std::list<RadosMetadata> &to_update, bool delete_source));
  MOCK_METHOD5(copy, bool(std::string &src_oid, const char *src_ns, std::string &dest_oid, const char *dest_ns,
                          std::list<RadosMetadata> &to_update));
  MOCK_METHOD2(save_mail, int(const std::string &oid, librados::bufferlist &bufferlist));
  MOCK_METHOD2(save_mail, bool(RadosMailObject *mail, bool &save_async));
  MOCK_METHOD0(alloc_mail_object, librmb::RadosMailObject *());
  MOCK_METHOD1(free_mail_object, void(librmb::RadosMailObject *mail));

  MOCK_METHOD2(update_extended_metadata, int(std::string &oid, librmb::RadosMetadata *metadata));
  MOCK_METHOD2(remove_extended_metadata, int(std::string &oid, std::string &key));
  MOCK_METHOD3(load_extended_metadata,
               int(std::string &oid, std::set<std::string> &keys, std::map<std::string, ceph::bufferlist> *metadata));
};

using librmb::RadosDictionary;

class RadosDictionaryMock : public RadosDictionary {
 public:
  MOCK_METHOD1(get_full_oid, const std::string(const std::string &key));
  MOCK_METHOD0(get_shared_oid, const std::string());
  MOCK_METHOD0(get_private_oid, const std::string());
  MOCK_METHOD0(get_oid, const std::string &());
  MOCK_METHOD0(get_username, const std::string &());
  MOCK_METHOD0(get_io_ctx, librados::IoCtx &());
  MOCK_METHOD1(remove_completion, void(librados::AioCompletion *c));
  MOCK_METHOD1(push_back_completion, void(librados::AioCompletion *c));
  MOCK_METHOD0(wait_for_completions, void());
  MOCK_METHOD2(get, int(const std::string &key, std::string *value_r));
};

using librmb::RadosCluster;

class RadosClusterMock : public RadosCluster {
 public:
  MOCK_METHOD0(init, int());
  MOCK_METHOD2(init, int(const std::string &clustername, const std::string &rados_username));

  MOCK_METHOD0(deinit, void());
  MOCK_METHOD1(pool_create, int(const std::string &pool));
  MOCK_METHOD2(io_ctx_create, int(const std::string &pool, librados::IoCtx *io_ctx));
  MOCK_METHOD2(get_config_option, int(const char *option, std::string *value));
  MOCK_METHOD0(is_connected, bool());
};

using librmb::RadosDovecotCephCfg;
class RadosDovecotCephCfgMock : public RadosDovecotCephCfg {
 public:
  // dovecot configuration
  MOCK_METHOD0(get_rados_cluster_name, const std::string &());
  MOCK_METHOD0(get_rados_username, const std::string &());

  // dovecot configuration
  MOCK_METHOD1(is_mail_attribute, bool(enum librmb::rbox_metadata_key key));
  MOCK_METHOD1(is_updateable_attribute, bool(enum librmb::rbox_metadata_key key));

  MOCK_METHOD1(update_mail_attributes, void(const char *value));
  MOCK_METHOD1(update_updatable_attributes, void(const char *value));
  MOCK_METHOD1(update_pool_name_metadata, void(const char *value));

  MOCK_METHOD0(get_mail_attributes_key, const std::string &());
  MOCK_METHOD0(get_updateable_attributes_key, const std::string &());
  MOCK_METHOD0(get_pool_name_metadata_key, const std::string &());
  MOCK_METHOD0(get_update_attributes_key, const std::string &());
  MOCK_METHOD0(get_config, std::map<std::string, std::string> *());

  MOCK_METHOD0(get_pool_name, std::string &());
  MOCK_METHOD0(is_update_attributes, bool());

  MOCK_METHOD2(update_metadata, void(const std::string &key, const char *value_));
  MOCK_METHOD0(is_config_valid, bool());
  MOCK_METHOD1(set_config_valid, void(bool is_valid_));
  MOCK_METHOD0(get_key_prefix_keywords, std::string &());
  MOCK_METHOD1(set_rbox_cfg_object_name, void(const std::string &value));

  // ceph configuration
  MOCK_METHOD1(set_io_ctx, void(librados::IoCtx *io_ctx));
  MOCK_METHOD0(load_rados_config, int());
  MOCK_METHOD0(save_default_rados_config, int());

  MOCK_METHOD1(set_user_mapping, void(bool value_));
  MOCK_METHOD0(is_user_mapping, bool());
  MOCK_METHOD1(set_user_ns, void(const std::string &ns));
  MOCK_METHOD0(get_user_ns, std::string &());
  MOCK_METHOD1(set_user_suffix, void(const std::string &ns_suffix));
  MOCK_METHOD0(get_user_suffix, std::string &());

  MOCK_METHOD0(get_public_namespace, const std::string &());
  MOCK_METHOD1(update_mail_attributes, void(const std::string &mail_attributes));

  MOCK_METHOD1(update_updatable_attributes, void(const std::string &updateable_attributes));
  MOCK_METHOD2(save_object, int(const std::string &oid, librados::bufferlist &buffer));
  MOCK_METHOD2(read_object, int(const std::string &oid, librados::bufferlist *buffer));
  MOCK_METHOD1(set_io_ctx_namespace, void(const std::string &namespace_));
};

}  // namespace librmbtest

#endif  // SRC_TESTS_MOCKS_MOCK_TEST_H_
