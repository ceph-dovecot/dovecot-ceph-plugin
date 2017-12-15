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
#include "gmock/gmock.h"
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

TEST_F(DictTest, deinit) {
  ASSERT_NE(target, nullptr);
  target->v.deinit(target);
  target = nullptr;
  i_free(set);
}

int main(int argc, char **argv) {
  // ::testing::InitGoogleTest(&argc, argv);
  ::testing::InitGoogleMock(&argc, argv);

  return RUN_ALL_TESTS();
}
