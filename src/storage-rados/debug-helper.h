/*
 * debug-helper.h
 *
 *  Created on: Apr 11, 2017
 *      Author: peter
 */

#ifndef DEBUG_HELPER_H_
#define DEBUG_HELPER_H_

struct rados_sync_context;
struct index_mail_data;

void debug_print_mail(struct mail *mail, const char *funcname, const char *mailName);
void debug_print_mailbox(struct mailbox *mailbox, const char *funcname, const char *boxName);
void debug_print_index_mail_data(struct index_mail_data *indexMailData, const char *funcname);
void debug_print_mail_save_context(struct mail_save_context *mailSaveContext, const char *funcname, const char* saveCtxName);
void debug_print_mailbox_transaction_context(struct mailbox_transaction_context* mailboxTransactionContext,
		const char *funcname, const char *ctxName);
void debug_print_mail_save_data(struct mail_save_data *mailSaveData, const char *funcname, const char *saveDataName);
void debug_print_mail_storage(struct mail_storage *mailStorage, const char *funcname, const char *storageName);
void debug_print_mail_user(struct mail_user *mailUser, const char *funcname, const char* userName);
void debug_print_rados_sync_context(struct rados_sync_context *radosSyncContext, const char *funcname);
void debug_print_mailbox_list(struct mailbox_list *mailboxList, const char *funcname, const char *listName);
void debug_print_mailbox_list_settings(struct mailbox_list_settings *mailboxListSettings, const char *funcname, const char *listSettingsName);
void debug_print_mail_index(struct mail_index *mailIndex, const char *funcname, const char *indexName);

#endif /* DEBUG_HELPER_H_ */
