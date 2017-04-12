/*
 * debug-helper.h
 *
 *  Created on: Apr 11, 2017
 *      Author: peter
 */

#ifndef DEBUG_HELPER_H_
#define DEBUG_HELPER_H_

void debug_print_mail(struct mail *mail, const char *funcname);
void debug_print_mailbox(struct mailbox *mailbox, const char *funcname);
void debug_print_index_mail_data(struct index_mail_data *indexMailData, const char *funcname);
void debug_print_mail_save_context(struct mail_save_context *mailSaveContext, const char *funcname);

#endif /* DEBUG_HELPER_H_ */
