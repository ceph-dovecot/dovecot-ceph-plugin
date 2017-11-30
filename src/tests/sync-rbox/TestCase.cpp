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
#define typeof(x) __typeof__(x)

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"           // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wundef"            // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wredundant-decls"  // turn off warnings for Dovecot :-(
#ifndef __cplusplus
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"  // turn off warnings for Dovecot :-(
#endif

extern "C" {
#include "lib.h"
#include "ioloop.h"
#include "master-service.h"
#include "master-service-settings.h"
#include "randgen.h"
#include "hostpid.h"
#include "mail-storage.h"
#include "mail-storage-service.h"
#include "settings-parser.h"
#include "unlink-directory.h"

#include "libstorage-rbox-plugin.h"
#include "array.h"
#include "array-decl.h"
}

#pragma GCC diagnostic pop

#if DOVECOT_PREREQ(2, 3)
#else
#define mail_storage_service_next(ctx, user, mail_user_r, error_r) mail_storage_service_next(ctx, user, mail_user_r)
#define unlink_directory(dir, flags, error_r) unlink_directory(dir, flags)
#endif

#ifndef i_zero
#define i_zero(p) memset(p, 0, sizeof(*(p)))
#endif

static const char *rbox_pool_name = "rbox_pool_name";

static int set_user_env(struct mail_user *user, const char *val) {
  const char *const *envs;
  unsigned int count, i;
  bool newly_created = false;

  if (!array_is_created(&user->set->plugin_envs)) {
    i_array_init(&user->set->plugin_envs, 2);
    newly_created = true;
  }

  if (!newly_created) {
    return 0;
  }
  array_append(&user->set->plugin_envs, &rbox_pool_name, 1);
  array_append(&user->set->plugin_envs, &val, 1);
  envs = array_get_modifiable(&user->set->plugin_envs, &count);

  for (i = 0; i < count; i += 2) {
    if (strcmp(envs[i], rbox_pool_name) == 0) {
      i_debug("found %s", envs[i + 1]);
      return 0;
    }
  }

  return -1;
}
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

static struct mail_storage_service_ctx *mail_storage_service = nullptr;
struct mail_user *SyncTest::s_test_mail_user = nullptr;
static struct mail_storage_service_user *test_service_user = nullptr;

rados_t SyncTest::s_cluster = nullptr;
rados_ioctx_t SyncTest::s_ioctx = nullptr;

std::string SyncTest::pool_name;  // NOLINT
std::string SyncTest::uri;        // NOLINT
struct ioloop *SyncTest::s_test_ioloop = nullptr;
pool_t SyncTest::s_test_pool = nullptr;
char *SyncTest::mail_home = NULL;
static const char *username = "user-rbox-test@domain";

void SyncTest::SetUpTestCase() {
  // prepare Ceph
  pool_name = get_temp_pool_name("test-storage-rbox-");
  ASSERT_EQ("", create_one_pool(pool_name, &s_cluster));
  ASSERT_EQ(0, rados_ioctx_create(s_cluster, pool_name.c_str(), &s_ioctx));

  // prepare Dovecot
  uri = "oid=metadata:pool=" + pool_name;

  const char *error;
  char path_buf[4096];

  char arg0[] = "storage-rbox-test";
  char *argv[] = {&arg0[0], NULL};
  auto a = &argv;
  int argc = static_cast<int>((sizeof(argv) / sizeof(argv[0])) - 1);

  master_service = master_service_init(
      "storage-rbox-test",
      (master_service_flags)(MASTER_SERVICE_FLAG_STANDALONE | MASTER_SERVICE_FLAG_NO_CONFIG_SETTINGS |
                             MASTER_SERVICE_FLAG_NO_SSL_INIT),
      &argc, reinterpret_cast<char ***>(&a), "");
  ASSERT_NE(master_service, nullptr);

  random_init();

  master_service_init_log(master_service, t_strdup_printf("storage_rbox(%s): ", my_pid));
  master_service_init_finish(master_service);

  s_test_pool = pool_alloconly_create(MEMPOOL_GROWING "storage-rbox-test-pool", 1024);
  s_test_ioloop = io_loop_create();

  ASSERT_NE(getcwd(path_buf, sizeof(path_buf)), nullptr);

  // mail_home = p_strdup_printf(s_test_pool, "%s/user-%s/", path_buf, pool_name.c_str());
  mail_home = p_strdup_printf(s_test_pool, "%s/%s/", path_buf, username);

  const char *const userdb_fields[] = {t_strdup_printf("mail=rbox:%s", mail_home),
                                       t_strdup_printf("home=%s", mail_home), NULL};

  struct mail_storage_service_input input;
  i_zero(&input);
  input.userdb_fields = userdb_fields;
  input.username = username;
  input.no_userdb_lookup = TRUE;
#if DOVECOT_PREREQ(2, 3)
  input.debug = TRUE;
#endif

  mail_storage_service = mail_storage_service_init(
      master_service, NULL,
      (mail_storage_service_flags)(MAIL_STORAGE_SERVICE_FLAG_NO_RESTRICT_ACCESS |
                                   MAIL_STORAGE_SERVICE_FLAG_NO_LOG_INIT | MAIL_STORAGE_SERVICE_FLAG_NO_PLUGINS));
  ASSERT_NE(mail_storage_service, nullptr);

  storage_rbox_plugin_init(0);

  ASSERT_GE(mail_storage_service_lookup(mail_storage_service, &input, &test_service_user, &error), 0);

  struct setting_parser_context *set_parser = mail_storage_service_user_get_settings_parser(test_service_user);
  ASSERT_NE(set_parser, nullptr);

  ASSERT_GE(
      settings_parse_line(set_parser, t_strdup_printf("mail_attribute_dict=file:%s/dovecot-attributes", mail_home)), 0);

  ASSERT_GE(mail_storage_service_next(mail_storage_service, test_service_user, &s_test_mail_user, &error), 0);
  set_user_env(s_test_mail_user, SyncTest::pool_name.c_str());
}

void SyncTest::TearDownTestCase() {
  if (array_is_created(&s_test_mail_user->set->plugin_envs)) {
    if (array_count(&s_test_mail_user->set->plugin_envs) > 0) {
      array_delete(&s_test_mail_user->set->plugin_envs, array_count(&s_test_mail_user->set->plugin_envs) - 1, 1);
    }
    array_free(&s_test_mail_user->set->plugin_envs);
  }
  mail_user_unref(&s_test_mail_user);
#if DOVECOT_PREREQ(2, 3)
  mail_storage_service_user_unref(&test_service_user);

  const char *error;
#else
  mail_storage_service_user_free(&test_service_user);
#endif

  storage_rbox_plugin_deinit();

  mail_storage_service_deinit(&mail_storage_service);
  EXPECT_GE(unlink_directory(mail_home, UNLINK_DIRECTORY_FLAG_RMDIR, &error), 0);

  io_loop_destroy(&s_test_ioloop);
  pool_unref(&s_test_pool);
  destroy_one_pool(pool_name, &s_cluster);
  rados_ioctx_destroy(s_ioctx);

  master_service_deinit(&master_service);
}

void SyncTest::SetUp() {}

void SyncTest::TearDown() {}
