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

#include "TestCase.h"

#include <errno.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"           // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wundef"            // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wredundant-decls"  // turn off warnings for Dovecot :-(
#ifndef __cplusplus
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"  // turn off warnings for Dovecot :-(
#endif

extern "C" {
#include "lib.h"
#include "dict-private.h"
#include "ioloop.h"
#include "master-service.h"
#include "master-service-settings.h"
#include "randgen.h"
#include "hostpid.h"

#include "libdict-rados-plugin.h"
}

#pragma GCC diagnostic pop
static std::string get_temp_pool_name(const std::string &prefix) {
  char hostname[80];
  char out[160];
  memset(hostname, 0, sizeof(hostname));
  memset(out, 0, sizeof(out));
  gethostname(hostname, sizeof(hostname) - 1);
  static int num = 1;
  snprintf(out, sizeof(out), "%s-%d-%d", hostname, getpid(), num);
  num++;
  return prefix + out;
}

static std::string connect_cluster(rados_t *cluster) {
  char *id = getenv("CEPH_CLIENT_ID");
  if (id)
    std::cerr << "Client id is: " << id << std::endl;

  int ret;
  ret = rados_create(cluster, NULL);
  if (ret) {
    std::ostringstream oss;
    oss << "rados_create failed with error " << ret;
    return oss.str();
  }
  ret = rados_conf_read_file(*cluster, NULL);
  if (ret) {
    rados_shutdown(*cluster);
    std::ostringstream oss;
    oss << "rados_conf_read_file failed with error " << ret;
    return oss.str();
  }
  rados_conf_parse_env(*cluster, NULL);
  ret = rados_connect(*cluster);
  if (ret) {
    rados_shutdown(*cluster);
    std::ostringstream oss;
    oss << "rados_connect failed with error " << ret;
    return oss.str();
  }
  return "";
}

static std::string create_one_pool(const std::string &pool_name, rados_t *cluster, uint32_t pg_num = 0) {
  std::string err_str = connect_cluster(cluster);
  if (err_str.length())
    return err_str;

  int ret = rados_pool_create(*cluster, pool_name.c_str());
  if (ret) {
    rados_shutdown(*cluster);
    std::ostringstream oss;
    oss << "create_one_pool(" << pool_name << ") failed with error " << ret;
    return oss.str();
  }

  return "";
}

static int destroy_one_pool(const std::string &pool_name, rados_t *cluster) {
  int ret = rados_pool_delete(*cluster, pool_name.c_str());
  if (ret) {
    rados_shutdown(*cluster);
    return ret;
  }
  rados_shutdown(*cluster);
  return 0;
}

rados_t DictTest::s_cluster = nullptr;
rados_ioctx_t DictTest::s_ioctx = nullptr;

std::string DictTest::pool_name;  // NOLINT
std::string DictTest::uri;        // NOLINT
struct ioloop *DictTest::s_test_ioloop = nullptr;
pool_t DictTest::s_test_pool = nullptr;

void DictTest::SetUpTestCase() {
  // prepare Ceph
  pool_name = get_temp_pool_name("test-dict-rados-");
  ASSERT_EQ("", create_one_pool(pool_name, &s_cluster));
  ASSERT_EQ(0, rados_ioctx_create(s_cluster, pool_name.c_str(), &s_ioctx));

  // prepare Dovecot
  uri = "oid=metadata:pool=" + pool_name;

  char arg0[] = "dict-rados-test";
  char *argv[] = {&arg0[0], NULL};
  auto a = &argv;
  int argc = static_cast<int>((sizeof(argv) / sizeof(argv[0])) - 1);

  master_service = master_service_init(
      "dict-rados-test",
      static_cast<master_service_flags>(MASTER_SERVICE_FLAG_STANDALONE | MASTER_SERVICE_FLAG_NO_CONFIG_SETTINGS |
                                        MASTER_SERVICE_FLAG_NO_SSL_INIT),
      &argc, reinterpret_cast<char ***>(&a), "");

  random_init();

  master_service_init_log(master_service, t_strdup_printf("dict(%s): ", my_pid));
  master_service_init_finish(master_service);

  s_test_pool = pool_alloconly_create(MEMPOOL_GROWING "dict-rados-test-pool", 64 * 1024);
  s_test_ioloop = io_loop_create();

  dict_rados_plugin_init(0);
}

void DictTest::TearDownTestCase() {
  dict_rados_plugin_deinit();

  io_loop_destroy(&s_test_ioloop);
  pool_unref(&s_test_pool);

  destroy_one_pool(pool_name, &s_cluster);
  rados_ioctx_destroy(s_ioctx);

  master_service_deinit(&master_service);
}

void DictTest::SetUp() {
  rados_remove(s_ioctx, "metadata/shared");
  rados_remove(s_ioctx, "metadata/username");
}

void DictTest::TearDown() {
  rados_remove(s_ioctx, "metadata/shared");
  rados_remove(s_ioctx, "metadata/username");
}
