
#ifndef DEBUG_HELPER_H_
#define DEBUG_HELPER_H_

struct rbox_sync_context;
struct index_mail_data;

#define btoa(x) ((x) ? "true" : "false")

#define FUNC_START() i_debug("[START] %s: %s at line %d", __FILE__, __func__, __LINE__)
#define FUNC_END() i_debug("[END] %s: %s at line %d\n", __FILE__, __func__, __LINE__)
#define FUNC_END_RET(ret) i_debug("[END] %s: %s at line %d, %s\n", __FILE__, __func__, __LINE__, ret)

void debug_print_mail(struct mail *target, const char *funcname, const char *name);
void debug_print_mailbox(struct mailbox *target, const char *funcname, const char *name);
void debug_print_rbox_mailbox(struct rbox_mailbox *target, const char *funcname, const char *name);
void debug_print_index_mail_data(struct index_mail_data *target, const char *funcname, const char *name);
void debug_print_mail_save_context(struct mail_save_context *target, const char *funcname, const char* name);
void debug_print_mailbox_transaction_context(struct mailbox_transaction_context* target, const char *funcname, const char *name);
void debug_print_mail_save_data(struct mail_save_data *target, const char *funcname, const char *name);
void debug_print_mail_storage(struct mail_storage *target, const char *funcname, const char *name);
void debug_print_mail_user(struct mail_user *target, const char *funcname, const char* name);
void debug_print_rbox_sync_context(struct rbox_sync_context *target, const char *funcname, const char *name);
void debug_print_mailbox_list(struct mailbox_list *mailboxList, const char *funcname, const char *name);
void debug_print_mailbox_list_settings(struct mailbox_list_settings *target, const char *funcname, const char *name);
void debug_print_mailbox_metadata(struct mailbox_metadata *target, const char *funcname, const char *name);
void debug_print_mail_index(struct mail_index *target, const char *funcname, const char *name);
void debug_print_rbox_mail_index_header(struct rbox_index_header *target, const char *funcname, const char *name);

#endif /* DEBUG_HELPER_H_ */
