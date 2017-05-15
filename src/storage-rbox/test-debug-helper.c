/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include "lib.h"
#include "failures.h"
#include "index-mail.h"
#include "mail-index-private.h"
#include "mailbox-list.h"
#include "mailbox-list-private.h"
#include "mail-storage.h"
#include "mail-storage-private.h"
#include "rbox-sync.h"
#include "debug-helper.h"

int main(int argc, char **argv) {
  (void)argc;
  (void)argv;

  int ret = 0;

  struct mail *mail = i_new(struct mail, 1);
  struct mailbox *mailbox = i_new(struct mailbox, 1);
  struct index_mail_data *index_mail_data = i_new(struct index_mail_data, 1);
  struct mail_save_context *mail_save_context = i_new(struct mail_save_context, 1);
  struct mail_user *mail_user = i_new(struct mail_user, 1);
  struct mailbox_list *mbox_lst = i_new(struct mailbox_list, 1);
  struct mailbox_list_settings *mbox_lst_settings = i_new(struct mailbox_list_settings, 1);

  mailbox->name = i_strdup("hburow");
  mailbox->flags = MAILBOX_FLAG_READONLY | MAILBOX_FLAG_KEEP_LOCKED;
  mailbox->open_error = MAIL_ERROR_EXISTS;

  mail->box = mailbox;
  mail->uid = 123;

  rbox_dbg_print_mail(mail, "test-debug-helper::main()", NULL);
  rbox_dbg_print_index_mail_data(index_mail_data, "test-debug-helper::main()", NULL);
  rbox_dbg_print_mail_save_context(mail_save_context, "test-debug-helper::main(1)", "empty 1");
  mail_save_context->dest_mail = mail;
  rbox_dbg_print_mail_save_context(mail_save_context, "test-debug-helper::main(2)", "empty 2");

  mail_user->_home = "/home/peter";
  mail_user->username = "hpburow";
  mail_user->admin = 1;
  mail_user->auth_user = "peter";
  mail_user->error = "None";
  mail_user->uid = 4711;
  rbox_dbg_print_mail_user(mail_user, "test-debug-helper::main()", NULL);

  mbox_lst_settings->escape_char = '8';
  mbox_lst_settings->inbox_path = "/var/log/mail";
  mbox_lst_settings->index_control_use_maildir_name = TRUE;
  rbox_dbg_print_mailbox_list_settings(mbox_lst_settings, "test-debug-helper::main()", NULL);

  struct mailbox_transaction_context *mailboxTransactionContext = i_new(struct mailbox_transaction_context, 1);
  rbox_dbg_print_mailbox_transaction_context(mailboxTransactionContext, "Empty transaction", "empty");
  struct mail_save_data *mailSaveData = i_new(struct mail_save_data, 1);
  rbox_dbg_print_mail_save_data(mailSaveData, "Empty save data", "empty");
  struct mail_storage *mailStorage = i_new(struct mail_storage, 1);
  rbox_dbg_print_mail_storage(mailStorage, "Empty storage", "empty");
  struct rbox_sync_context *rboxSyncContext = i_new(struct rbox_sync_context, 1);
  rbox_dbg_print_rbox_sync_context(rboxSyncContext, "Empty context", NULL);
  struct mail_index *mailIndex = i_new(struct mail_index, 1);
  rbox_dbg_print_mail_index(mailIndex, "Empty mail index", "empty");
  rbox_dbg_print_mailbox_list(mbox_lst, "Empty mailbox list", "empty");

  i_debug("END");

  i_free(mailboxTransactionContext);
  i_free(mailSaveData);
  i_free(mailStorage);
  i_free(rboxSyncContext);
  i_free(mailIndex);

  i_free(mbox_lst_settings);
  i_free(mbox_lst);
  i_free(mail_user);
  i_free(mail_save_context);
  i_free(index_mail_data);
  i_free(mail->box);
  i_free(mail);

  return ret;
}
