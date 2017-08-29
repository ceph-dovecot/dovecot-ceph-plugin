/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

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
#include "dict-private.h"
#include "ioloop.h"

#include "libdict-rados-plugin.h"
}

#pragma GCC diagnostic pop

#if DOVECOT_PREREQ(2, 3)
#define dict_lookup(dict, pool, key, value_r, error_r) dict_driver_rados.v.lookup(dict, pool, key, value_r, error_r)
#define dict_iterate_deinit(ctx, error_r) dict_driver_rados.v.iterate_deinit(ctx, error_r)
#else
#define dict_lookup(dict, pool, key, value_r, error_r) dict_driver_rados.v.lookup(dict, pool, key, value_r)
#define dict_iterate_deinit(ctx, error_r) dict_driver_rados.v.iterate_deinit(ctx)
#endif

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
static struct dict *test_dict = nullptr;
static const char *error_r = nullptr;

TEST_F(DictTest, dict_init) {
  set = i_new(struct dict_settings, 1);
  set->username = "username";

  ASSERT_EQ(dict_driver_rados.v.init(&dict_driver_rados, uri.c_str(), set, &test_dict, &error_r), 0);
}

TEST_F(DictTest, dict_deinit) {
  test_dict->v.deinit(test_dict);
  i_free(set);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
