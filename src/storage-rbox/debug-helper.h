/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#ifndef SRC_STORAGE_RBOX_DEBUG_HELPER_H_
#define SRC_STORAGE_RBOX_DEBUG_HELPER_H_

struct rbox_sync_context;
struct index_mail_data;

#define btoa(x) ((x) ? "true" : "false")

#define PRINT_FUNC

#ifdef PRINT_FUNC
#define FUNC_START() i_debug("[START] %s: %s at line %d", __FILE__, __func__, __LINE__)
#define FUNC_START_PID() i_debug("[START (%d)] %s: %s at line %d", getpid(), __FILE__, __func__, __LINE__)
#define FUNC_END() i_debug("[END] %s: %s at line %d\n", __FILE__, __func__, __LINE__)
#define FUNC_END_RET(ret) i_debug("[END] %s: %s at line %d, %s\n", __FILE__, __func__, __LINE__, ret)
#else
#define FUNC_START()
#define FUNC_START_PID()
#define FUNC_END()
#define FUNC_END_RET(ret)
#endif

void print_trace(void);

const char *unixdate2str(time_t timestamp);

void debug_print_mail(struct mail *target, const char *funcname, const char *name);
void debug_print_mailbox(struct mailbox *target, const char *funcname, const char *name);
void debug_print_rbox_mailbox(struct rbox_mailbox *target, const char *funcname, const char *name);
void debug_print_index_mail_data(struct index_mail_data *target, const char *funcname, const char *name);
void debug_print_mail_save_context(struct mail_save_context *target, const char *funcname, const char *name);
void debug_print_mailbox_transaction_context(struct mailbox_transaction_context *target, const char *funcname,
                                             const char *name);
void debug_print_mail_save_data(struct mail_save_data *target, const char *funcname, const char *name);
void debug_print_mail_storage(struct mail_storage *target, const char *funcname, const char *name);
void debug_print_mail_user(struct mail_user *target, const char *funcname, const char *name);
void debug_print_rbox_sync_context(struct rbox_sync_context *target, const char *funcname, const char *name);
void debug_print_mailbox_list(struct mailbox_list *mailboxList, const char *funcname, const char *name);
void debug_print_mailbox_list_settings(struct mailbox_list_settings *target, const char *funcname, const char *name);
void debug_print_mailbox_metadata(struct mailbox_metadata *target, const char *funcname, const char *name);
void debug_print_mail_index(struct mail_index *target, const char *funcname, const char *name);
void debug_print_sdbox_index_header(struct sdbox_index_header *target, const char *funcname, const char *name);
void debug_print_obox_mail_index_record(struct obox_mail_index_record *target, const char *funcname, const char *name);

#endif /* SRC_STORAGE_RBOX_DEBUG_HELPER_H_ */
