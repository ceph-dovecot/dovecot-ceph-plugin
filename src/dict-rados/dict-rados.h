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

#ifndef SRC_DICT_RADOS_DICT_RADOS_H_
#define SRC_DICT_RADOS_DICT_RADOS_H_

extern int rados_dict_init(struct dict *driver, const char *uri, const struct dict_settings *set, struct dict **dict_r,
                           const char **error_r);
extern void rados_dict_deinit(struct dict *_dict);

#if DOVECOT_PREREQ(2, 3, 0)
extern void rados_dict_wait(struct dict *dict);
extern int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r,
                             const char **error_r);
#else
extern int rados_dict_wait(struct dict *dict);
extern int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r);
#endif

extern void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback,
                                    void *context);
extern struct dict_transaction_context *rados_dict_transaction_init(struct dict *_dict);
#if DOVECOT_PREREQ(2, 3, 0)
extern void rados_dict_transaction_commit(struct dict_transaction_context *_ctx, bool async,
                                          dict_transaction_commit_callback_t *callback, void *context);
#else
extern int rados_dict_transaction_commit(struct dict_transaction_context *_ctx, bool async,
                                         dict_transaction_commit_callback_t *callback, void *context);
#endif
extern void rados_dict_transaction_rollback(struct dict_transaction_context *_ctx);

extern void rados_dict_set(struct dict_transaction_context *_ctx, const char *key, const char *value);
extern void rados_dict_unset(struct dict_transaction_context *_ctx, const char *key);
extern void rados_dict_atomic_inc(struct dict_transaction_context *_ctx, const char *key, long long diff);  // NOLINT

extern struct dict_iterate_context *rados_dict_iterate_init(struct dict *_dict, const char *const *paths,
                                                            enum dict_iterate_flags flags);
extern bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r);
#if DOVECOT_PREREQ(2, 3, 0)
extern int rados_dict_iterate_deinit(struct dict_iterate_context *ctx, const char **error_r);
#else
extern int rados_dict_iterate_deinit(struct dict_iterate_context *ctx);
#endif

extern void rados_dict_set_timestamp(struct dict_transaction_context *ctx, const struct timespec *ts);

#endif  // SRC_DICT_RADOS_DICT_RADOS_H_
