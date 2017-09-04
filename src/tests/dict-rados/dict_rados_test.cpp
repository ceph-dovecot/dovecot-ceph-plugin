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

#include "gtest/gtest.h"
#include "TestCase.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"           // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wundef"            // turn off warnings for Dovecot :-(
#pragma GCC diagnostic ignored "-Wredundant-decls"  // turn off warnings for Dovecot :-(
#ifndef __cplusplus
#pragma GCC diagnostic ignored "-Wdeclaration-after-statement"  // turn off warnings for Dovecot :-(
#endif

extern "C" {
#include "lib.h"
#include "dict.h"
#include "dict-private.h"
#include "ioloop.h"

#include "libdict-rados-plugin.h"
}

#pragma GCC diagnostic pop

#if DOVECOT_PREREQ(2, 3)
#define dict_lookup(dict, pool, key, value_r, error_r) dict_lookup(dict, pool, key, value_r, error_r)
#define dict_iterate_deinit(ctx, error_r) dict_iterate_deinit(ctx, error_r)
#define test_dict_transaction_commit(ctx, error_r) dict_transaction_commit(ctx, error_r)

#define EXPECT_KVEQ(k, v)                                          \
  {                                                                \
    const char *v_r;                                               \
    const char *e_r;                                               \
    ASSERT_EQ(dict_lookup(target, s_test_pool, k, &v_r, &e_r), 1); \
    EXPECT_STREQ(v, v_r);                                          \
  }
#else
#define dict_lookup(dict, pool, key, value_r, error_r) dict_lookup(dict, pool, key, value_r)
#define dict_iterate_deinit(ctx, error_r) dict_iterate_deinit(ctx)
#define dict_transaction_commit(ctx, error_r) dict_transaction_commit(ctx)

#define EXPECT_KVEQ(k, v)                                             \
  {                                                                   \
    const char *v_r;                                                  \
    ASSERT_EQ(dict_lookup(target, s_test_pool, k, &v_r, nullptr), 1); \
    EXPECT_STREQ(v, v_r);                                             \
  }
#endif

enum rados_commit_ret {
  RADOS_COMMIT_RET_OK = 1,
  RADOS_COMMIT_RET_NOTFOUND = 0,
  RADOS_COMMIT_RET_FAILED = -1,
};

static const char *OMAP_KEY_PRIVATE = "priv/key";
static const char *OMAP_VALUE_PRIVATE = "PRIVATE";
static const char *OMAP_KEY_SHARED = "shared/key";
static const char *OMAP_VALUE_SHARED = "SHARED";

static const char *OMAP_KEY_ATOMIC_INC = "priv/atomic_inc";
static const char *OMAP_KEY_ATOMIC_INC_NOT_FOUND = "priv/atomic_inc_not_found";

static const char *OMAP_ITERATE_EXACT_KEYS[] = {"priv/K1", "priv/K2", "priv/K3", "priv/K4", "shared/S1", NULL};
static const char *OMAP_ITERATE_EXACT_VALUES[] = {"V1", "V2", "V3", "V4", "VS1", NULL};

static const char *OMAP_ITERATE_KEYS[] = {"priv/A1",    "priv/A1/B1", "priv/A/B1/C1", "priv/A1/B1/C2",
                                          "priv/A1/B2", "priv/A2",    "shared/S1",    NULL};
static const char *OMAP_ITERATE_VALUES[] = {"V-A1",    "V-A1/B1", "V-A/B1/C1", "V-A1/B1/C2",
                                            "V-A1/B2", "V-A2",    "V-S1",      NULL};
static const char *OMAP_ITERATE_KEY[] = {"priv/A1/", "shared/S1", NULL};
static const char *OMAP_ITERATE_RESULTS[] = {"V-A1/B1", "V-A1/B2", "V-S1", NULL};

static const char *OMAP_ITERATE_REC_RESULTS[] = {"V-A1/B1", "V-A1/B1/C2", "V-A1/B2", "V-S1", NULL};

extern struct dict dict_driver_rados;
static struct dict_settings *set;
static struct dict *target = nullptr;
static const char *error_r;

TEST_F(DictTest, init) {
  set = i_new(struct dict_settings, 1);
  set->username = "username";

  ASSERT_EQ(dict_driver_rados.v.init(&dict_driver_rados, uri.c_str(), set, &target, &error_r), 0);
}

TEST_F(DictTest, lookup_not_found) {
  ASSERT_NE(target, nullptr);
  const char *value_r;

  EXPECT_EQ(dict_lookup(target, s_test_pool, "priv/dict_lookup_not_found", &value_r, &error_r), 0);
  EXPECT_EQ(dict_lookup(target, s_test_pool, "shared/dict_lookup_not_found", &value_r, &error_r), 0);
}

TEST_F(DictTest, lookup) {
  ASSERT_NE(target, nullptr);

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  dict_set(ctx, OMAP_KEY_PRIVATE, OMAP_VALUE_PRIVATE);
  dict_set(ctx, OMAP_KEY_SHARED, OMAP_VALUE_SHARED);
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);
  ASSERT_EQ(ctx, nullptr);

  EXPECT_KVEQ(OMAP_KEY_PRIVATE, OMAP_VALUE_PRIVATE);
  EXPECT_KVEQ(OMAP_KEY_SHARED, OMAP_VALUE_SHARED);
}

static void test_lookup_callback(const struct dict_lookup_result *result, void *context) {
  if (result->error != NULL) {
    i_error("test_lookup_callback(): error=%s", result->error);
  } else if (result->ret != 1) {
    i_error("test_lookup_callback(): ret=%d", result->ret);
  } else {
    i_debug("test_lookup_callback(): value=%s", result->value);
    if (context != NULL) {
      *reinterpret_cast<char **>(context) = p_strdup(DictTest::get_test_pool(), result->value);
    }
  }
}

TEST_F(DictTest, lookup_async) {
  ASSERT_NE(target, nullptr);
  const char *value_private = nullptr;
  const char *value_shared = nullptr;

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  dict_set(ctx, OMAP_KEY_PRIVATE, OMAP_VALUE_PRIVATE);
  dict_set(ctx, OMAP_KEY_SHARED, OMAP_VALUE_SHARED);
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  dict_lookup_async(target, OMAP_KEY_PRIVATE, test_lookup_callback, &value_private);
  dict_lookup_async(target, OMAP_KEY_SHARED, test_lookup_callback, &value_shared);
  dict_wait(target);

  ASSERT_STREQ(OMAP_VALUE_PRIVATE, value_private);
  ASSERT_STREQ(OMAP_VALUE_SHARED, value_shared);
}

TEST_F(DictTest, iterate) {
  ASSERT_NE(target, nullptr);
  int i;

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  i = 0;
  for (auto k = OMAP_ITERATE_KEYS; *k != NULL; k++) {
    dict_set(ctx, *k, OMAP_ITERATE_VALUES[i++]);
  }
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  struct dict_iterate_context *iter = dict_iterate_init_multiple(target, OMAP_ITERATE_KEY, dict_iterate_flags(0));

  i = 0;
  const char *kr;
  const char *vr;
  while (dict_iterate(iter, &kr, &vr)) {
    EXPECT_STREQ(vr, OMAP_ITERATE_RESULTS[i]);
    i++;
  }
  EXPECT_EQ(i, 3);
  ASSERT_EQ(dict_iterate_deinit(&iter, &error_r), 0);
}

TEST_F(DictTest, iterate_exact_key) {
  ASSERT_NE(target, nullptr);
  int i;

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  for (i = 0; OMAP_ITERATE_EXACT_KEYS[i] != NULL; i++) {
    dict_set(ctx, OMAP_ITERATE_EXACT_KEYS[i], OMAP_ITERATE_EXACT_VALUES[i]);
  }
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  struct dict_iterate_context *iter =
      dict_iterate_init_multiple(target, OMAP_ITERATE_EXACT_KEYS, dict_iterate_flags(DICT_ITERATE_FLAG_EXACT_KEY));

  i = 0;
  const char *k, *v;
  while (dict_iterate(iter, &k, &v)) {
    EXPECT_STREQ(k, OMAP_ITERATE_EXACT_KEYS[i]);
    EXPECT_STREQ(v, OMAP_ITERATE_EXACT_VALUES[i]);
    i++;
  }
  EXPECT_EQ(i, 5);
  ASSERT_EQ(dict_iterate_deinit(&iter, &error_r), 0);
}

TEST_F(DictTest, iterate_exact_key_no_value) {
  ASSERT_NE(target, nullptr);
  int i;

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  for (i = 0; OMAP_ITERATE_EXACT_KEYS[i] != NULL; i++) {
    dict_set(ctx, OMAP_ITERATE_EXACT_KEYS[i], OMAP_ITERATE_EXACT_VALUES[i]);
  }
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  struct dict_iterate_context *iter = dict_iterate_init_multiple(
      target, OMAP_ITERATE_EXACT_KEYS, dict_iterate_flags(DICT_ITERATE_FLAG_NO_VALUE | DICT_ITERATE_FLAG_EXACT_KEY));

  i = 0;
  const char *k, *v;
  while (dict_iterate(iter, &k, &v)) {
    EXPECT_STREQ(k, OMAP_ITERATE_EXACT_KEYS[i]);
    EXPECT_STREQ(v, 0);
    i++;
  }
  EXPECT_EQ(i, 5);
  ASSERT_EQ(dict_iterate_deinit(&iter, &error_r), 0);
}

TEST_F(DictTest, iterate_recursive) {
  ASSERT_NE(target, nullptr);
  int i;

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  i = 0;
  for (auto k = OMAP_ITERATE_KEYS; *k != NULL; k++) {
    dict_set(ctx, *k, OMAP_ITERATE_VALUES[i++]);
  }
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  struct dict_iterate_context *iter =
      dict_iterate_init_multiple(target, OMAP_ITERATE_KEY, dict_iterate_flags(DICT_ITERATE_FLAG_RECURSE));

  i = 0;
  const char *kr;
  const char *vr;
  while (dict_iterate(iter, &kr, &vr)) {
    EXPECT_STREQ(vr, OMAP_ITERATE_REC_RESULTS[i]);
    i++;
  }
  EXPECT_EQ(i, 4);
  EXPECT_EQ(dict_iterate_deinit(&iter, &error_r), 0);
}

#if DOVECOT_PREREQ(2, 3)
static void test_dict_transaction_commit_callback(const struct dict_commit_result *result, void *context) {
  if (context != NULL) {
    *reinterpret_cast<int *>(context) = result->ret;
  }
}
#else
static void test_dict_transaction_commit_callback(int ret, void *context) {
  if (context != NULL) {
    *reinterpret_cast<int *>(context) = ret;
  }
}
#endif

TEST_F(DictTest, transaction_commit_async) {
  ASSERT_NE(target, nullptr);
  int result = 0;

  struct dict_transaction_context *ctx = dict_transaction_begin(target);

  dict_set(ctx, OMAP_KEY_PRIVATE, OMAP_VALUE_PRIVATE);
  dict_set(ctx, OMAP_KEY_SHARED, OMAP_VALUE_SHARED);

  dict_transaction_commit_async(&ctx, test_dict_transaction_commit_callback, &result);

  dict_wait(target);

  ASSERT_EQ(result, 1);

  EXPECT_KVEQ(OMAP_KEY_PRIVATE, OMAP_VALUE_PRIVATE);
  EXPECT_KVEQ(OMAP_KEY_SHARED, OMAP_VALUE_SHARED);
}

TEST_F(DictTest, transaction_multiple) {
  ASSERT_NE(target, nullptr);

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  dict_set(ctx, OMAP_KEY_PRIVATE, "0");
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  struct dict_transaction_context *ctx1 = dict_transaction_begin(target);
  dict_set(ctx1, OMAP_KEY_PRIVATE, "1");

  EXPECT_KVEQ(OMAP_KEY_PRIVATE, "0");

  struct dict_transaction_context *ctx2 = dict_transaction_begin(target);
  dict_set(ctx2, OMAP_KEY_PRIVATE, "2");

  ASSERT_EQ(dict_transaction_commit(&ctx2, &error_r), 1);
  ASSERT_EQ(dict_transaction_commit(&ctx1, &error_r), 1);

  EXPECT_KVEQ(OMAP_KEY_PRIVATE, "1");
}

TEST_F(DictTest, atomic_inc) {
  ASSERT_NE(target, nullptr);

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  dict_set(ctx, OMAP_KEY_ATOMIC_INC, "10");
  dict_transaction_commit(&ctx, &error_r);

  ctx = dict_transaction_begin(target);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, 10);
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  EXPECT_KVEQ(OMAP_KEY_ATOMIC_INC, "20");
}

TEST_F(DictTest, atomic_inc_async) {
  ASSERT_NE(target, nullptr);
  int result_r = 0;

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  dict_set(ctx, OMAP_KEY_ATOMIC_INC, "10");
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, 10);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, 10);
  dict_transaction_commit_async(&ctx, test_dict_transaction_commit_callback, &result_r);
  dict_wait(target);
  ASSERT_EQ(result_r, 1);

  EXPECT_KVEQ(OMAP_KEY_ATOMIC_INC, "30");
}

TEST_F(DictTest, atomic_inc_not_found) {
  ASSERT_NE(target, nullptr);

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC_NOT_FOUND, 99);
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), RADOS_COMMIT_RET_NOTFOUND);
}

TEST_F(DictTest, atomic_inc_multiple) {
  ASSERT_NE(target, nullptr);

  struct dict_transaction_context *ctx = dict_transaction_begin(target);
  dict_set(ctx, OMAP_KEY_ATOMIC_INC, "10");
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  ctx = dict_transaction_begin(target);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, 10);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, 10);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, -10);
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 1);

  EXPECT_KVEQ(OMAP_KEY_ATOMIC_INC, "20");

  ctx = dict_transaction_begin(target);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, -10);
  dict_unset(ctx, OMAP_KEY_ATOMIC_INC);
  dict_atomic_inc(ctx, OMAP_KEY_ATOMIC_INC, 10);
  ASSERT_EQ(dict_transaction_commit(&ctx, &error_r), 0);
}

TEST_F(DictTest, deinit) {
  ASSERT_NE(target, nullptr);
  target->v.deinit(target);
  target = nullptr;
  i_free(set);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
