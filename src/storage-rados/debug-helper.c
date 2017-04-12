/*
 * debug-helper.c
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

#define btoa(x) ((x) ? "true" : "false")

static char *enum_mail_access_type_strs[] = {"MAIL_ACCESS_TYPE_DEFAULT", "MAIL_ACCESS_TYPE_SEARCH", "MAIL_ACCESS_TYPE_SORT"};
static char *enum_mail_lookup_abort_strs[] = {
		"MAIL_LOOKUP_ABORT_NEVER", "MAIL_LOOKUP_ABORT_READ_MAIL", "MAIL_LOOKUP_ABORT_NOT_IN_CACHE"};
static char *enum_mail_error_strs[] = {"MAIL_ERROR_NONE", "MAIL_ERROR_TEMP", "MAIL_ERROR_NOTPOSSIBLE", "MAIL_ERROR_PARAMS",
		"MAIL_ERROR_PERM", "MAIL_ERROR_NOQUOTA", "MAIL_ERROR_NOTFOUND", "MAIL_ERROR_EXISTS", "MAIL_ERROR_EXPUNGED",
		"MAIL_ERROR_INUSE", "MAIL_ERROR_CONVERSION", "MAIL_ERROR_INVALIDDATA", "MAIL_ERROR_LIMIT", "MAIL_ERROR_LOOKUP_ABORTED"};

void debug_print_mail(struct mail *mail, const char *funcname) {

	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mail == NULL) {
		i_debug("mail = NULL");
	} else {
		i_debug("mail uid = %d", mail->uid);
		i_debug("mail seq = %d", mail->seq);

		i_debug("mail expunged = %d", mail->expunged);
		i_debug("mail saving = %d", mail->saving);
		i_debug("mail has_nuls = %d", mail->has_nuls);
		i_debug("mail has_no_nuls = %d", mail->has_no_nuls);

		i_debug("mail stream_opened = %s", btoa(mail->mail_stream_opened));
		i_debug("mail metadata_accessed = %s", btoa(mail->mail_metadata_accessed));

		i_debug("mail access_type = %s", enum_mail_access_type_strs[mail->access_type]);
		i_debug("mail lookup_abort = %s", enum_mail_lookup_abort_strs[mail->lookup_abort]);

		if (mail->box != NULL) {
			debug_print_mailbox(mail->box, NULL);
		}
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}

}

void debug_print_mailbox(struct mailbox *mailbox, const char *funcname) {

	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mailbox == NULL) {
		i_debug("mailbox = NULL");
	} else {
		i_debug("mailbox name = %s", mailbox->name);
		i_debug("mailbox vname = %s", mailbox->vname);
		i_debug("mailbox path = %s", mailbox->_path);
		i_debug("mailbox index_path = %s", mailbox->_index_path);
		i_debug("mailbox open_error = %s", enum_mail_error_strs[mailbox->open_error]);
		i_debug("mailbox index_prefix = %s", mailbox->index_prefix);
		i_debug("mailbox flags = 0x%04x", mailbox->flags);
		i_debug("mailbox transaction_count = %d", mailbox->transaction_count);
		i_debug("mailbox enabled_features = 0x%04x", mailbox->enabled_features);
		i_debug("mailbox generation_sequence = %u", mailbox->generation_sequence);
		i_debug("mailbox opened = %u", mailbox->opened);
		i_debug("mailbox mailbox_deleted = %u", mailbox->mailbox_deleted);
		i_debug("mailbox creating = %u", mailbox->creating);
		i_debug("mailbox deleting = %u", mailbox->deleting);
		i_debug("mailbox mailbox_undeleting = %u", mailbox->mailbox_undeleting);
		i_debug("mailbox delete_sync_check = %u", mailbox->delete_sync_check);
		i_debug("mailbox deleting_must_be_empty = %u", mailbox->deleting_must_be_empty);
		i_debug("mailbox delete_skip_empty_check = %u", mailbox->delete_skip_empty_check);
		i_debug("mailbox marked_deleted = %u", mailbox->marked_deleted);
		i_debug("mailbox marked_deleted = %u", mailbox->marked_deleted);
		i_debug("mailbox inbox_user = %u", mailbox->inbox_user);
		i_debug("mailbox inbox_any = %u", mailbox->inbox_any);
		i_debug("mailbox disable_reflink_copy_to = %u", mailbox->disable_reflink_copy_to);
		i_debug("mailbox disallow_new_keywords = %u", mailbox->disallow_new_keywords);
		i_debug("mailbox synced = %u", mailbox->synced);
		i_debug("mailbox mail_cache_disabled = %u", mailbox->mail_cache_disabled);
		i_debug("mailbox update_first_saved = %u", mailbox->update_first_saved);
		i_debug("mailbox skip_create_name_restrictions = %u", mailbox->skip_create_name_restrictions);
		i_debug("mailbox list_index_has_changed_quick = %u", mailbox->list_index_has_changed_quick);
		i_debug("mailbox corrupted_mailbox_name = %u", mailbox->corrupted_mailbox_name);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}

}

void debug_print_index_mail_data(struct index_mail_data *indexMailData, const char *funcname) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (indexMailData == NULL) {
		i_debug("index_mail_data = NULL");
	} else {
		i_debug("index_mail_data date = %ld", indexMailData->date);
		i_debug("index_mail_data received_date = %ld", indexMailData->received_date);
		i_debug("index_mail_data save_date = %ld", indexMailData->save_date);

		i_debug("index_mail_data virtual_size = %lu", indexMailData->virtual_size);
		i_debug("index_mail_data physical_size = %lu", indexMailData->physical_size);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_save_context(struct mail_save_context *mailSaveContext, const char *funcname) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mailSaveContext == NULL) {
		i_debug("mail_save_context = NULL");
	} else {
		i_debug("mail_save_context unfinished = %u", mailSaveContext->unfinished);
		i_debug("mail_save_context finishing = %u", mailSaveContext->finishing);
		i_debug("mail_save_context copying_via_save = %u", mailSaveContext->copying_via_save);
		i_debug("mail_save_context saving = %u", mailSaveContext->saving);
		i_debug("mail_save_context moving = %u", mailSaveContext->moving);
		i_debug("mail_save_context copying_or_moving = %u", mailSaveContext->copying_or_moving);
		i_debug("mail_save_context dest_mail_external = %u", mailSaveContext->dest_mail_external);
		debug_print_mail(mailSaveContext->dest_mail, NULL);
		debug_print_mail(mailSaveContext->copy_src_mail, NULL);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

