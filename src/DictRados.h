/*
 * DictRados.h
 *
 *  Created on: Jan 25, 2017
 *      Author: peter
 */

#ifndef SRC_DICTRADOS_H_
#define SRC_DICTRADOS_H_

#include "lib.h"
#include "dict-private.h"

int rados_dict_init(struct dict *driver, const char *uri, const struct dict_settings *set, struct dict **dict_r,
		const char **error_r);
void rados_dict_deinit(struct dict *_dict);

int rados_dict_lookup(struct dict *_dict, pool_t pool, const char *key, const char **value_r);
void rados_dict_lookup_async(struct dict *_dict, const char *key, dict_lookup_callback_t *callback, void *context);

struct dict_transaction_context *rados_transaction_init(struct dict *_dict);
int rados_transaction_commit(struct dict_transaction_context *_ctx, bool async, dict_transaction_commit_callback_t *callback,
		void *context);
void rados_transaction_rollback(struct dict_transaction_context *_ctx);

void rados_set(struct dict_transaction_context *_ctx, const char *key, const char *value);
void rados_unset(struct dict_transaction_context *_ctx, const char *key);
void rados_atomic_inc(struct dict_transaction_context *_ctx, const char *key, long long diff);

struct dict_iterate_context *rados_dict_iterate_init(struct dict *_dict, const char * const *paths, enum dict_iterate_flags flags);
bool rados_dict_iterate(struct dict_iterate_context *ctx, const char **key_r, const char **value_r);
int rados_dict_iterate_deinit(struct dict_iterate_context *ctx);

#endif /* SRC_DICTRADOS_H_ */
