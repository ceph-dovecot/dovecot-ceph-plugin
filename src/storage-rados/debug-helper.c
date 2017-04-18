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
#include "mailbox-list.h"
#include "mailbox-list-private.h"
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
static char *enum_file_lock_method[] = {"FILE_LOCK_METHOD_FCNTL", "FILE_LOCK_METHOD_FLOCK",	"FILE_LOCK_METHOD_DOTLOCK"};

void debug_print_mail(struct mail *mail, const char *funcname, const char *mailName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (mailName != NULL) {
		i_debug("mail %s", mailName);
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

		//debug_print_mailbox(mail->box, NULL, "box");
		i_debug("mail box = %p", mail->box);
		//debug_print_mailbox_transaction_context(mail->transaction, NULL, "transaction");
		i_debug("mail transaction = %p", mail->transaction);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mailbox(struct mailbox *mailbox, const char *funcname, const char *boxName) {

	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (boxName != NULL) {
		i_debug("mailbox %s", boxName);
	}
	if (mailbox == NULL) {
		i_debug("mailbox = NULL");
	} else {
		i_debug("mailbox name = %s", mailbox->name);
		i_debug("mailbox vname = %s", mailbox->vname);
		i_debug("mailbox pool = %p", mailbox->pool);
		i_debug("mailbox metadata_pool = %p", mailbox->metadata_pool);
		i_debug("mailbox prev = %p", mailbox->prev);
		i_debug("mailbox next = %p", mailbox->next);
		i_debug("mailbox view = %p", mailbox->view);
		i_debug("mailbox cache = %p", mailbox->cache);
		i_debug("mailbox view_pvt = %p", mailbox->view_pvt);
		i_debug("mailbox set = %p", mailbox->set);
		i_debug("mailbox input = %p", mailbox->input);
		i_debug("mailbox tmp_sync_view = %p", mailbox->tmp_sync_view);
		i_debug("mailbox notify_callback = %p", mailbox->notify_callback);
		i_debug("mailbox notify_context = %p", mailbox->notify_context);
		i_debug("mailbox to_notify = %p", mailbox->to_notify);
		i_debug("mailbox to_notify_delay = %p", mailbox->to_notify_delay);
		i_debug("mailbox notify_files = %p", mailbox->notify_files);
		i_debug("mailbox recent_flags size = %ld", mailbox->recent_flags.arr.element_size);
		i_debug("mailbox search_results size = %ld", mailbox->search_results.arr.element_size);
		i_debug("mailbox module_contexts size = %ld", mailbox->module_contexts.arr.element_size);
		i_debug("mailbox _perm.file_uid = %u", mailbox->_perm.file_uid);
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

		//debug_print_mail_storage(mailbox->storage, NULL, "storage");
		i_debug("mailbox storage = %p", mailbox->storage);
		//debug_print_mailbox_list(mailbox->list, NULL, "list");
		i_debug("mailbox list = %p", mailbox->list);
		//debug_print_mail_index(mailbox->index, NULL, "index");
		i_debug("mailbox index = %p", mailbox->index);
		//debug_print_mail_index(mailbox->index_pvt, NULL, "index_pvt");
		i_debug("mailbox index_pvt = %p", mailbox->index_pvt);
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

		i_debug("index_mail_data parts = %p", indexMailData->parts);
		i_debug("index_mail_data bin_parts = %p", indexMailData->bin_parts);
		i_debug("index_mail_data envelope_data = %p", indexMailData->envelope_data);
		i_debug("index_mail_data wanted_headers = %p", indexMailData->wanted_headers);
		i_debug("index_mail_data search_results = %p", indexMailData->search_results);
		i_debug("index_mail_data stream = %p", indexMailData->stream);
		i_debug("index_mail_data filter_stream = %p", indexMailData->filter_stream);
		i_debug("index_mail_data tee_stream = %p", indexMailData->tee_stream);
		i_debug("index_mail_data parser_input = %p", indexMailData->parser_input);
		i_debug("index_mail_data parser_ctx = %p", indexMailData->parser_ctx);

		i_debug("index_mail_data keywords size = %ld", indexMailData->keywords.arr.element_size);
		i_debug("index_mail_data keyword_indexes size = %ld", indexMailData->keyword_indexes.arr.element_size);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_save_context(struct mail_save_context *mailSaveContext, const char *funcname, const char* saveCtxName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (saveCtxName != NULL) {
		i_debug("mail_save_context %s", saveCtxName);
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
		i_debug("mail_save_context data.uid = %u", mailSaveContext->data.uid);

		//debug_print_mail(mailSaveContext->dest_mail, NULL, "dest_mail");
		i_debug("mail_save_context dest_mail = %p", mailSaveContext->dest_mail);
		//debug_print_mail(mailSaveContext->copy_src_mail, NULL, "copy_src_mail");
		i_debug("mail_save_context copy_src_mail = %p", mailSaveContext->copy_src_mail);
		//debug_print_mailbox_transaction_context(mailSaveContext->transaction, NULL, "transaction");
		i_debug("mail_save_context transaction = %p", mailSaveContext->transaction);
		//debug_print_mail_save_data(&mailSaveContext->data, NULL, "data");
		i_debug("mail_save_context data = %p", mailSaveContext->data);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mailbox_transaction_context(struct mailbox_transaction_context* mailboxTransactionContext,
		const char *funcname, const char *ctxName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (ctxName != NULL) {
		i_debug("mailbox_transaction_context %s", ctxName);
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

		i_debug("mailbox_transaction_context itrans = %p", mailboxTransactionContext->itrans);
		i_debug("mailbox_transaction_context attr_pvt_trans = %p", mailboxTransactionContext->attr_pvt_trans);
		i_debug("mailbox_transaction_context attr_shared_trans = %p", mailboxTransactionContext->attr_shared_trans);
		i_debug("mailbox_transaction_context view = %p", mailboxTransactionContext->view);
		i_debug("mailbox_transaction_context itrans_pvt = %p", mailboxTransactionContext->itrans_pvt);
		i_debug("mailbox_transaction_context view_pvt = %p", mailboxTransactionContext->view_pvt);
		i_debug("mailbox_transaction_context cache_view = %p", mailboxTransactionContext->cache_view);
		i_debug("mailbox_transaction_context cache_trans = %p", mailboxTransactionContext->cache_trans);
		i_debug("mailbox_transaction_context changes = %p", mailboxTransactionContext->changes);
		i_debug("mailbox_transaction_context stats.cache_hit_count = %lu", mailboxTransactionContext->stats.cache_hit_count);
		i_debug("mailbox_transaction_context module_contexts size = %ld", mailboxTransactionContext->module_contexts.arr.element_size);
		i_debug("mailbox_transaction_context pvt_saves size = %ld", mailboxTransactionContext->pvt_saves.arr.element_size);

		//debug_print_mailbox(mailboxTransactionContext->box, NULL, "box");
		i_debug("mailbox_transaction_context box = %p", mailboxTransactionContext->box);
		//debug_print_mail_save_context(mailboxTransactionContext->save_ctx, NULL, "save_ctx");
		i_debug("mailbox_transaction_context save_ctx = %p", mailboxTransactionContext->save_ctx);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_save_data(struct mail_save_data *mailSaveData, const char *funcname, const char *saveDataName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (saveDataName != NULL) {
		i_debug("mail_save_data %s", saveDataName);
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

		i_debug("mail_save_data attach = %p", mailSaveData->attach);
		i_debug("mail_save_data keywords = %p", mailSaveData->keywords);
		i_debug("mail_save_data output = %p", mailSaveData->output);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_storage(struct mail_storage *mailStorage, const char *funcname, const char *storageName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (storageName != NULL) {
		i_debug("mail_storage %s", storageName);
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

		i_debug("mail_storage prev = %p", mailStorage->prev);
		i_debug("mail_storage next = %p", mailStorage->next);
		i_debug("mail_storage mailboxes = %p", mailStorage->mailboxes);
		i_debug("mail_storage storage_class = %p", mailStorage->storage_class);
		i_debug("mail_storage set = %p", mailStorage->set);
		i_debug("mail_storage callback_context = %p", mailStorage->callback_context);
		i_debug("mail_storage _shared_attr_dict = %p", mailStorage->_shared_attr_dict);

		i_debug("mail_storage error_stack size = %ld", mailStorage->error_stack.arr.element_size);
		i_debug("mail_storage module_contexts size = %ld", mailStorage->module_contexts.arr.element_size);

		//debug_print_mail_user(mailStorage->user, NULL, "user");
		i_debug("mail_storage user = %p", mailStorage->user);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_user(struct mail_user *mailUser, const char *funcname, const char* userName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (userName != NULL) {
		i_debug("mail_user %s", userName);
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
		i_debug("mail_user userdb_fields = %s", mailUser->userdb_fields != NULL ? *mailUser->userdb_fields : "NULL");
		i_debug("mail_user error = %s", mailUser->error);
		i_debug("mail_user session_create_time = %ld", mailUser->session_create_time);

		i_debug("mail_user creator = %p", mailUser->creator);
		i_debug("mail_user _service_user = %p", mailUser->_service_user);
		i_debug("mail_user local_ip = %p", mailUser->local_ip);
		i_debug("mail_user remote_ip = %p", mailUser->remote_ip);
		i_debug("mail_user var_expand_table = %p", mailUser->var_expand_table);
		i_debug("mail_user set_info = %p", mailUser->set_info);
		i_debug("mail_user unexpanded_set = %p", mailUser->unexpanded_set);
		i_debug("mail_user set = %p", mailUser->set);
		i_debug("mail_user namespaces = %p", mailUser->namespaces);
		i_debug("mail_user storages = %p", mailUser->storages);
		i_debug("mail_user default_normalizer = %p", mailUser->default_normalizer);
		i_debug("mail_user _attr_dict = %p", mailUser->_attr_dict);

		i_debug("mail_user hooks size = %ld", mailUser->hooks.arr.element_size);
		i_debug("mail_user module_contexts size = %ld", mailUser->module_contexts.arr.element_size);

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
		i_debug("rados_sync_context path = %s", radosSyncContext->path != NULL ? (char *) radosSyncContext->path->data : "NULL");
		i_debug("rados_sync_context path_dir_prefix_len = %lu", radosSyncContext->path_dir_prefix_len);
		i_debug("rados_sync_context uid_validity = %u", radosSyncContext->uid_validity);

		i_debug("rados_sync_context index_sync_ctx = %p", radosSyncContext->index_sync_ctx);
		i_debug("rados_sync_context sync_view = %p", radosSyncContext->sync_view);
		i_debug("rados_sync_context trans = %p", radosSyncContext->trans);

		//debug_print_mailbox(&radosSyncContext->mbox->box, NULL, "mbox->box");
		i_debug("rados_sync_context mbox->box = %p", radosSyncContext->mbox->box);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mailbox_list(struct mailbox_list *mailboxList, const char *funcname, const char *listName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (listName != NULL) {
		i_debug("mailbox_list %s", listName);
	}
	if (mailboxList == NULL) {
		i_debug("mailbox_list = NULL");
	} else {
		i_debug("mailbox_list name = %s", mailboxList->name);
		i_debug("mailbox_list props = 0x%04x", mailboxList->props);
		i_debug("mailbox_list mailbox_name_max_length = %lu", mailboxList->mailbox_name_max_length);
		if (mailboxList->pool != NULL) {
			i_debug("mail_user pool name = %s", mailboxList->pool->v->get_name(mailboxList->pool));
		}
		if (mailboxList->guid_cache_pool != NULL) {
			i_debug("mail_user pool name = %s", mailboxList->guid_cache_pool->v->get_name(mailboxList->guid_cache_pool));
		}
		i_debug("mailbox_list mail_set = %p", mailboxList->mail_set);
		i_debug("mailbox_list subscriptions = %p", mailboxList->subscriptions);
		i_debug("mailbox_list changelog = %p", mailboxList->changelog);

		i_debug("mailbox_list root_permissions.file_uid = %u", mailboxList->root_permissions.file_uid);
		i_debug("mailbox_list module_contexts size = %ld", mailboxList->module_contexts.arr.element_size);

		i_debug("mailbox_list subscriptions_mtime = %ld", mailboxList->subscriptions_mtime);
		i_debug("mailbox_list subscriptions_read_time = %ld", mailboxList->subscriptions_read_time);
		i_debug("mailbox_list changelog_timestamp = %ld", mailboxList->changelog_timestamp);
		i_debug("mailbox_list guid_cache_pool = %p", mailboxList->guid_cache_pool);
		i_debug("mailbox_list guid_cache_errors = %s", btoa(mailboxList->guid_cache_errors));
		i_debug("mailbox_list error_string = %s", mailboxList->error_string);
		i_debug("mailbox_list error = %s", enum_mail_error_strs[mailboxList->error]);
		i_debug("mailbox_list temporary_error = %s", btoa(mailboxList->temporary_error));
		i_debug("mailbox_list index_root_dir_created = %u", mailboxList->index_root_dir_created);
		i_debug("mailbox_list guid_cache_updated = %u", mailboxList->guid_cache_updated);
		i_debug("mailbox_list guid_cache_invalidated = %u", mailboxList->guid_cache_invalidated);

		//debug_print_mailbox_list_settings(&mailboxList->set, NULL, "set");
		i_debug("mailbox_list set = %p", mailboxList->set);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mailbox_list_settings(struct mailbox_list_settings *mailboxListSettings, const char *funcname, const char *listSettingsName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (listSettingsName != NULL) {
		i_debug("mailbox_list_settings %s", listSettingsName);
	}
	if (mailboxListSettings == NULL) {
		i_debug("mailbox_list_settings = NULL");
	} else {
		i_debug("mailbox_list_settings layout = %s", mailboxListSettings->layout);
		i_debug("mailbox_list_settings root_dir = %s", mailboxListSettings->root_dir);
		i_debug("mailbox_list_settings index_dir = %s", mailboxListSettings->index_dir);
		i_debug("mailbox_list_settings index_pvt_dir = %s", mailboxListSettings->index_pvt_dir);
		i_debug("mailbox_list_settings control_dir = %s", mailboxListSettings->control_dir);
		i_debug("mailbox_list_settings alt_dir = %s", mailboxListSettings->alt_dir);
		i_debug("mailbox_list_settings inbox_path = %s", mailboxListSettings->inbox_path);
		i_debug("mailbox_list_settings subscription_fname = %s", mailboxListSettings->subscription_fname);
		i_debug("mailbox_list_settings list_index_fname = %s", mailboxListSettings->list_index_fname);
		i_debug("mailbox_list_settings maildir_name = %s", mailboxListSettings->maildir_name);
		i_debug("mailbox_list_settings mailbox_dir_name = %s", mailboxListSettings->mailbox_dir_name);
		i_debug("mailbox_list_settings escape_char = %c", mailboxListSettings->escape_char);
		i_debug("mailbox_list_settings broken_char = %c", mailboxListSettings->broken_char);
		i_debug("mailbox_list_settings utf8 = %s", btoa(mailboxListSettings->utf8));
		i_debug("mailbox_list_settings alt_dir_nocheck = %s", btoa(mailboxListSettings->alt_dir_nocheck));
		i_debug("mailbox_list_settings index_control_use_maildir_name = %s", btoa(mailboxListSettings->index_control_use_maildir_name));
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}

void debug_print_mail_index(struct mail_index *mailIndex, const char *funcname, const char *indexName) {
	if (funcname != NULL) {
		i_debug("### %s", funcname);
	}
	if (indexName != NULL) {
		i_debug("mail_index %s", indexName);
	}
	if (mailIndex == NULL) {
		i_debug("mail_index = NULL");
	} else {
		i_debug("mail_index dir = %s", mailIndex->dir);
		i_debug("mail_index prefix = %s", mailIndex->prefix);
		i_debug("mail_index cache = %p", mailIndex->cache);
		i_debug("mail_index log = %p", mailIndex->log);
		i_debug("mail_index open_count = %u", mailIndex->open_count);
		i_debug("mail_index flags = 0x%04x", mailIndex->flags);
		i_debug("mail_index fsync_mode = 0x%04x", mailIndex->fsync_mode);
		i_debug("mail_index fsync_mask = 0x%04x", mailIndex->fsync_mask);
		i_debug("mail_index mode = %d", mailIndex->mode);
		i_debug("mail_index gid = %d", mailIndex->gid);
		i_debug("mail_index gid_origin = %s", mailIndex->gid_origin);
		i_debug("mail_index log_rotate_min_size = %lu", mailIndex->log_rotate_min_size);
		i_debug("mail_index log_rotate_max_size = %lu", mailIndex->log_rotate_max_size);
		i_debug("mail_index log_rotate_min_created_ago_secs = %u", mailIndex->log_rotate_min_created_ago_secs);
		i_debug("mail_index log_rotate_log2_stale_secs = %u", mailIndex->log_rotate_log2_stale_secs);
		i_debug("mail_index extension_pool = %p", mailIndex->extension_pool);
		i_debug("mail_index ext_hdr_init_id = %u", mailIndex->ext_hdr_init_id);
		i_debug("mail_index ext_hdr_init_data = %p", mailIndex->ext_hdr_init_data);
		i_debug("mail_index filepath = %s", mailIndex->filepath);
		i_debug("mail_index fd = %d", mailIndex->fd);
		i_debug("mail_index map = %p", mailIndex->map);
		i_debug("mail_index last_mmap_error_time = %ld", mailIndex->last_mmap_error_time);
		i_debug("mail_index indexid = %u", mailIndex->indexid);
		i_debug("mail_index inconsistency_id = %u", mailIndex->inconsistency_id);
		i_debug("mail_index last_read_log_file_seq = %u", mailIndex->last_read_log_file_seq);
		i_debug("mail_index last_read_log_file_tail_offset = %u", mailIndex->last_read_log_file_tail_offset);
		i_debug("mail_index fsck_log_head_file_seq = %u", mailIndex->fsck_log_head_file_seq);
		i_debug("mail_index fsck_log_head_file_offset = %lu", mailIndex->fsck_log_head_file_offset);
		i_debug("mail_index sync_commit_result = %p", mailIndex->sync_commit_result);
		i_debug("mail_index lock_method = %s", enum_file_lock_method[mailIndex->lock_method]);
		i_debug("mail_index max_lock_timeout_secs = %u", mailIndex->max_lock_timeout_secs);
		i_debug("mail_index keywords_pool = %p", mailIndex->keywords_pool);
		i_debug("mail_index keywords_ext_id = %u", mailIndex->keywords_ext_id);
		i_debug("mail_index modseq_ext_id = %u", mailIndex->modseq_ext_id);
		i_debug("mail_index views = %p", mailIndex->views);
		i_debug("mail_index error = %s", mailIndex->error);
		i_debug("mail_index nodiskspace = %u", mailIndex->nodiskspace);
		i_debug("mail_index index_lock_timeout = %u", mailIndex->index_lock_timeout);
		i_debug("mail_index index_delete_requested = %u", mailIndex->index_delete_requested);
		i_debug("mail_index index_deleted = %u", mailIndex->index_deleted);
		i_debug("mail_index log_sync_locked = %u", mailIndex->log_sync_locked);
		i_debug("mail_index readonly = %u", mailIndex->readonly);
		i_debug("mail_index mapping = %u", mailIndex->mapping);
		i_debug("mail_index syncing = %u", mailIndex->syncing);
		i_debug("mail_index need_recreate = %u", mailIndex->need_recreate);
		i_debug("mail_index index_min_write = %u", mailIndex->index_min_write);
		i_debug("mail_index modseqs_enabled = %u", mailIndex->modseqs_enabled);
		i_debug("mail_index initial_create = %u", mailIndex->initial_create);
		i_debug("mail_index initial_mapped = %u", mailIndex->initial_mapped);
		i_debug("mail_index fscked = %u", mailIndex->fscked);

		i_debug("mail_index cache = %p", mailIndex->cache);
		i_debug("mail_index log = %p", mailIndex->log);
		i_debug("mail_index ext_hdr_init_data = %p", mailIndex->ext_hdr_init_data);
		i_debug("mail_index map = %p", mailIndex->map);
		i_debug("mail_index sync_commit_result = %p", mailIndex->sync_commit_result);
		i_debug("mail_index views = %p", mailIndex->views);
		i_debug("mail_index extension_pool = %p", mailIndex->extension_pool);
		i_debug("mail_index keywords_pool = %p", mailIndex->keywords_pool);

		i_debug("mail_index extensions size = %ld", mailIndex->extensions.arr.element_size);
		i_debug("mail_index sync_lost_handlers size = %ld", mailIndex->sync_lost_handlers.arr.element_size);
		i_debug("mail_index keywords size = %ld", mailIndex->keywords.arr.element_size);
		i_debug("mail_index module_contexts size = %ld", mailIndex->module_contexts.arr.element_size);
	}
	if (funcname != NULL) {
		i_debug("###\n");
	}
}
