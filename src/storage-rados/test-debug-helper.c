/*
 * test-debug-helper.c
 *
 *  Created on: Apr 11, 2017
 *      Author: peter
 */

#include "lib.h"
#include "failures.h"
#include "index-mail.h"
#include "mail-storage.h"
#include "mail-storage-private.h"
#include "debug-helper.h"

int main(int argc, char **argv) {
	int ret = 0;

	struct mail *mail = i_new(struct mail, 1);
	struct mailbox *mailbox = i_new(struct mailbox, 1);
	struct index_mail_data *index_mail_data = i_new(struct index_mail_data, 1);
	struct mail_save_context *mail_save_context = i_new(struct mail_save_context, 1);

	mailbox->name = i_strdup("hburow");
	mailbox->flags = MAILBOX_FLAG_READONLY | MAILBOX_FLAG_KEEP_LOCKED;
	mailbox->open_error = MAIL_ERROR_EXISTS;

	mail->box = mailbox;
	mail->uid = 123;

	debug_print_mail(mail, "test-debug-helper::main()");
	debug_print_mail(mail, "test-debug-helper::test()");
	debug_print_index_mail_data(index_mail_data, "test-debug-helper::main()");
	debug_print_mail_save_context(mail_save_context, "test-debug-helper::main(1)");
	mail_save_context->dest_mail = mail;
	debug_print_mail_save_context(mail_save_context, "test-debug-helper::main(2)");

	i_free(mail_save_context);
	i_free(index_mail_data);
	i_free(mail->box);
	i_free(mail);

	return ret;

}


