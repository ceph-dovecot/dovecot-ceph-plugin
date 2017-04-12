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
#include "rados-storage.h"
#include "rados-sync.h"
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

		debug_print_mailbox(mail->box, NULL);
		debug_print_mailbox_transaction_context(mail->transaction, NULL);
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

		debug_print_mail_storage(mailbox->storage, NULL);
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
		debug_print_mailbox_transaction_context(mailSaveContext->transaction, NULL);
		debug_print_mail_save_data(&mailSaveContext->data, NULL);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mailbox_transaction_context(struct mailbox_transaction_context* mailboxTransactionContext, const char *funcname) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mailboxTransactionContext == NULL) {
		i_debug("mailbox_transaction_context = NULL");
	} else {
		i_debug("mailbox_transaction_context flags = 0x%04x", mailboxTransactionContext->flags);
		i_debug("mailbox_transaction_context mail_ref_count = %d", mailboxTransactionContext->mail_ref_count);
		i_debug("mailbox_transaction_context prev_pop3_uidl_tracking_seq = %u", mailboxTransactionContext->prev_pop3_uidl_tracking_seq);
		i_debug("mailbox_transaction_context highest_pop3_uidl_uid = %u", mailboxTransactionContext->highest_pop3_uidl_uid);
		i_debug("mailbox_transaction_context save_count = %u", mailboxTransactionContext->save_count);
		i_debug("mailbox_transaction_context stats_track = %u", mailboxTransactionContext->stats_track);
		i_debug("mailbox_transaction_context nontransactional_changes = %u", mailboxTransactionContext->nontransactional_changes);
		i_debug("mailbox_transaction_context internal_attribute = %d", mailboxTransactionContext->internal_attribute);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_save_data(struct mail_save_data *mailSaveData, const char *funcname) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mailSaveData == NULL) {
		i_debug("mail_save_data = NULL");
	} else {
		i_debug("mail_save_data flags = 0x%04x", mailSaveData->flags);
		i_debug("mail_save_data pvt_flags = 0x%04x", mailSaveData->pvt_flags);
		i_debug("mail_save_data min_modseq = %lu", mailSaveData->min_modseq);
		i_debug("mail_save_data received_date = %ld", mailSaveData->received_date);
		i_debug("mail_save_data save_date = %ld", mailSaveData->save_date);
		i_debug("mail_save_data received_tz_offset = %d", mailSaveData->received_tz_offset);
		i_debug("mail_save_data guid = %s", mailSaveData->guid);
		i_debug("mail_save_data pop3_uidl = %s", mailSaveData->pop3_uidl);
		i_debug("mail_save_data from_envelope = %s", mailSaveData->from_envelope);
		i_debug("mail_save_data pop3_order = %u", mailSaveData->pop3_order);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_storage(struct mail_storage *mailStorage, const char *funcname) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mailStorage == NULL) {
		i_debug("mail_storage = NULL");
	} else {
		i_debug("mail_storage name = %s", mailStorage->name);
		i_debug("mail_storage flags = 0x%04x", mailStorage->flags);
		i_debug("mail_storage class_flags = 0x%04x", mailStorage->class_flags);
		if (mailStorage->pool != NULL) {
			i_debug("mail_storage pool name = %s", mailStorage->pool->v->get_name(mailStorage->pool));
		}
		i_debug("mail_storage refcount = %d", mailStorage->refcount);
		i_debug("mail_storage obj_refcount = %d", mailStorage->obj_refcount);
		i_debug("mail_storage unique_root_dir = %s", mailStorage->unique_root_dir);
		i_debug("mail_storage error_string = %s", mailStorage->error_string);
		i_debug("mail_storage error = %s", enum_mail_error_strs[mailStorage->error]);
		i_debug("mail_storage temp_path_prefix = %s", mailStorage->temp_path_prefix);
		i_debug("mail_storage shared_attr_dict_failed = %u", mailStorage->shared_attr_dict_failed);

		debug_print_mail_user(mailStorage->user, NULL);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_user(struct mail_user *mailUser, const char *funcname) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mailUser == NULL) {
		i_debug("mail_user = NULL");
	} else {
		if (mailUser->pool != NULL) {
			i_debug("mail_user pool name = %s", mailUser->pool->v->get_name(mailUser->pool));
		}
		i_debug("mail_user refcount = %d", mailUser->refcount);
		i_debug("mail_user username = %s", mailUser->username);
		i_debug("mail_user _home = %s", mailUser->_home);
		i_debug("mail_user uid = %u", mailUser->uid);
		i_debug("mail_user gid = %u", mailUser->gid);
		i_debug("mail_user service = %s", mailUser->service);
		i_debug("mail_user session_id = %s", mailUser->session_id);
		i_debug("mail_user auth_token = %s", mailUser->auth_token);
		i_debug("mail_user auth_user = %s", mailUser->auth_user);
		i_debug("mail_user userdb_fields = %s", *mailUser->userdb_fields);
		i_debug("mail_user error = %s", mailUser->error);
		i_debug("mail_user session_create_time = %ld", mailUser->session_create_time);

		i_debug("mail_user nonexistent = %u", mailUser->nonexistent);
		i_debug("mail_user home_looked_up = %u", mailUser->home_looked_up);
		i_debug("mail_user anonymous = %u", mailUser->anonymous);
		i_debug("mail_user autocreated = %u", mailUser->autocreated);
		i_debug("mail_user initialized = %u", mailUser->initialized);
		i_debug("mail_user namespaces_created = %u", mailUser->namespaces_created);
		i_debug("mail_user settings_expanded = %u", mailUser->settings_expanded);
		i_debug("mail_user mail_debug = %u", mailUser->mail_debug);
		i_debug("mail_user inbox_open_error_logged = %u", mailUser->inbox_open_error_logged);
		i_debug("mail_user fuzzy_search = %u", mailUser->fuzzy_search);
		i_debug("mail_user dsyncing = %u", mailUser->dsyncing);
		i_debug("mail_user attr_dict_failed = %u", mailUser->attr_dict_failed);
		i_debug("mail_user deinitializing = %u", mailUser->deinitializing);
		i_debug("mail_user admin = %u", mailUser->admin);
		i_debug("mail_user stats_enabled = %u", mailUser->stats_enabled);
		i_debug("mail_user autoexpunge_enabled = %u", mailUser->autoexpunge_enabled);
		i_debug("mail_user session_restored = %u", mailUser->session_restored);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_rados_sync_context(struct rados_sync_context *radosSyncContext, const char *funcname) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (radosSyncContext == NULL) {
		i_debug("rados_sync_context = NULL");
	} else {
		i_debug("rados_sync_context path = %s", (char *) radosSyncContext->path->data);
		i_debug("rados_sync_context path_dir_prefix_len = %lu", radosSyncContext->path_dir_prefix_len);
		i_debug("rados_sync_context uid_validity = %u", radosSyncContext->uid_validity);

		debug_print_mailbox(&radosSyncContext->mbox->box, NULL);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}
