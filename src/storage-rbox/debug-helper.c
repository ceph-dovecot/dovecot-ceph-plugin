/* Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file */

#include <execinfo.h>
#include <time.h>

#include "lib.h"
#include "failures.h"
#include "index-mail.h"
#include "mail-storage.h"
#include "mail-storage-private.h"
#include "mailbox-list.h"
#include "mailbox-list-private.h"
#include "rbox-storage.hpp"
#include "rbox-sync.h"
#include "debug-helper.h"

// #define RBOX_DEBUG

static char *enum_mail_access_type_strs[] = {"MAIL_ACCESS_TYPE_DEFAULT", "MAIL_ACCESS_TYPE_SEARCH",
                                             "MAIL_ACCESS_TYPE_SORT"};
static char *enum_mail_lookup_abort_strs[] = {"MAIL_LOOKUP_ABORT_NEVER", "MAIL_LOOKUP_ABORT_READ_MAIL",
                                              "MAIL_LOOKUP_ABORT_NOT_IN_CACHE"};
static char *enum_mail_error_strs[] = {"MAIL_ERROR_NONE",     "MAIL_ERROR_TEMP",          "MAIL_ERROR_NOTPOSSIBLE",
                                       "MAIL_ERROR_PARAMS",   "MAIL_ERROR_PERM",          "MAIL_ERROR_NOQUOTA",
                                       "MAIL_ERROR_NOTFOUND", "MAIL_ERROR_EXISTS",        "MAIL_ERROR_EXPUNGED",
                                       "MAIL_ERROR_INUSE",    "MAIL_ERROR_CONVERSION",    "MAIL_ERROR_INVALIDDATA",
                                       "MAIL_ERROR_LIMIT",    "MAIL_ERROR_LOOKUP_ABORTED"};
static char *enum_file_lock_method[] = {"FILE_LOCK_METHOD_FCNTL", "FILE_LOCK_METHOD_FLOCK", "FILE_LOCK_METHOD_DOTLOCK"};

#ifdef RBOX_DEBUG
#define RBOX_PRINT_START(NAME)                \
  if (funcname == NULL)                       \
    funcname = "-";                           \
  if (name == NULL)                           \
    name = NAME;                              \
  if (target == NULL)                         \
    i_debug("%s: %s = NULL", funcname, name); \
  else {
#define RBOX_PRINT_DEBUG(FORMAT, ...) \
  ;                                   \
  i_debug("%s: %s." FORMAT, funcname, name, __VA_ARGS__)
#define RBOX_PRINT_END() }
#else
#define RBOX_PRINT_START(NAME) \
  (void)target;                \
  (void)funcname;              \
  (void)name;
#define RBOX_PRINT_DEBUG(FORMAT, ...)
#define RBOX_PRINT_END()
#endif

#define STRFTIME_MAX_BUFSIZE (1024 * 64)

static const char *strftime_real(const char *fmt, const struct tm *tm) {
  size_t bufsize = strlen(fmt) + 32;
  char *buf = t_buffer_get(bufsize);
  size_t ret;

  while ((ret = strftime(buf, bufsize, fmt, tm)) == 0) {
    bufsize *= 2;
    i_assert(bufsize <= STRFTIME_MAX_BUFSIZE);
    buf = t_buffer_get(bufsize);
  }
  t_buffer_alloc(ret + 1);
  return buf;
}

static const char *t_strflocaltime(const char *fmt, time_t t) { return strftime_real(fmt, localtime(&t)); }

const char *unixdate2str(time_t timestamp) { return t_strflocaltime("%Y-%m-%d %H:%M:%S", timestamp); }

void debug_print_mail(struct mail *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mail")

  RBOX_PRINT_DEBUG("uid = %d", target->uid)
  RBOX_PRINT_DEBUG("seq = %d", target->seq);

  RBOX_PRINT_DEBUG("expunged = %d", target->expunged);
  RBOX_PRINT_DEBUG("saving = %d", target->saving);
  RBOX_PRINT_DEBUG("has_nuls	= %d", target->has_nuls);
  RBOX_PRINT_DEBUG("has_no_nuls = %d", target->has_no_nuls);

  RBOX_PRINT_DEBUG("stream_opened = %s", btoa(target->mail_stream_opened));
  RBOX_PRINT_DEBUG("metadata_accessed = %s", btoa(target->mail_metadata_accessed));

  RBOX_PRINT_DEBUG("access_type = %s", enum_mail_access_type_strs[target->access_type]);
  RBOX_PRINT_DEBUG("lookup_abort = %s", enum_mail_lookup_abort_strs[target->lookup_abort]);

  // debug_print_mailbox_transaction_context(target->transaction, NULL, "transaction");
  RBOX_PRINT_DEBUG("transaction = %p", target->transaction);

  // RBOX_PRINT_DEBUG("box = %p", target->box);
  debug_print_mailbox(target->box, funcname, t_strdup_printf("  %s.%s", name, "box"));

  RBOX_PRINT_END()
}

void debug_print_sdbox_index_header(struct sdbox_index_header *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("sdbox_index_header")

  RBOX_PRINT_DEBUG("rebuild_count = %u", target->rebuild_count);
  RBOX_PRINT_DEBUG("mailbox_guid = %s", guid_128_to_string(target->mailbox_guid));
  RBOX_PRINT_DEBUG("flags = 0x%x", target->flags);

  RBOX_PRINT_END()
}

void debug_print_obox_mail_index_record(struct obox_mail_index_record *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("obox_mail_index_record")

  RBOX_PRINT_DEBUG("guid = %s", guid_128_to_string(target->guid));
  RBOX_PRINT_DEBUG("oid = %s", guid_128_to_string(target->oid));

  RBOX_PRINT_END()
}

void debug_print_mailbox_metadata(struct mailbox_metadata *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mailbox_metadata")

  RBOX_PRINT_DEBUG("guid = %s", guid_128_to_string(target->guid));
  RBOX_PRINT_DEBUG("virtual_size = %lu", target->virtual_size);
  RBOX_PRINT_DEBUG("physical_size = %lu", target->physical_size);
  RBOX_PRINT_DEBUG("first_save_date = %lu (%s)", target->first_save_date, unixdate2str(target->first_save_date));

  RBOX_PRINT_END()
}

void debug_print_mailbox(struct mailbox *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mailbox")

  RBOX_PRINT_DEBUG("name = %s", target->name);
  RBOX_PRINT_DEBUG("vname = %s", target->vname);
  RBOX_PRINT_DEBUG("pool = %p", target->pool);
  RBOX_PRINT_DEBUG("metadata_pool = %p", target->metadata_pool);
  RBOX_PRINT_DEBUG("prev = %p", target->prev);
  RBOX_PRINT_DEBUG("next = %p", target->next);
  RBOX_PRINT_DEBUG("view = %p", target->view);
  RBOX_PRINT_DEBUG("cache = %p", target->cache);
  RBOX_PRINT_DEBUG("view_pvt = %p", target->view_pvt);
  RBOX_PRINT_DEBUG("set = %p", target->set);
  RBOX_PRINT_DEBUG("input = %p", target->input);
  RBOX_PRINT_DEBUG("tmp_sync_view = %p", target->tmp_sync_view);
  RBOX_PRINT_DEBUG("notify_callback = %p", target->notify_callback);
  RBOX_PRINT_DEBUG("notify_context = %p", target->notify_context);
  RBOX_PRINT_DEBUG("to_notify = %p", target->to_notify);
  RBOX_PRINT_DEBUG("to_notify_delay = %p", target->to_notify_delay);
  RBOX_PRINT_DEBUG("notify_files = %p", target->notify_files);
  RBOX_PRINT_DEBUG("recent_flags size = %ld", target->recent_flags.arr.element_size);
  RBOX_PRINT_DEBUG("search_results size = %ld", target->search_results.arr.element_size);
  RBOX_PRINT_DEBUG("module_contexts size = %ld", target->module_contexts.arr.element_size);
  RBOX_PRINT_DEBUG("_perm.file_uid = %u", target->_perm.file_uid);
  RBOX_PRINT_DEBUG("_path = %s", target->_path);
  RBOX_PRINT_DEBUG("_index_path = %s", target->_index_path);
  RBOX_PRINT_DEBUG("open_error = %s", enum_mail_error_strs[target->open_error]);
  RBOX_PRINT_DEBUG("index_prefix = %s", target->index_prefix);
  RBOX_PRINT_DEBUG("flags = 0x%04x", target->flags);
  RBOX_PRINT_DEBUG("transaction_count = %d", target->transaction_count);
  RBOX_PRINT_DEBUG("enabled_features = 0x%04x", target->enabled_features);
  RBOX_PRINT_DEBUG("generation_sequence = %u", target->generation_sequence);
  RBOX_PRINT_DEBUG("opened = %u", target->opened);
  RBOX_PRINT_DEBUG("mailbox_deleted = %u", target->mailbox_deleted);
  RBOX_PRINT_DEBUG("creating = %u", target->creating);
  RBOX_PRINT_DEBUG("deleting = %u", target->deleting);
  RBOX_PRINT_DEBUG("mailbox_undeleting = %u", target->mailbox_undeleting);
  RBOX_PRINT_DEBUG("delete_sync_check = %u", target->delete_sync_check);
  RBOX_PRINT_DEBUG("deleting_must_be_empty = %u", target->deleting_must_be_empty);
  RBOX_PRINT_DEBUG("delete_skip_empty_check = %u", target->delete_skip_empty_check);
  RBOX_PRINT_DEBUG("marked_deleted = %u", target->marked_deleted);
  RBOX_PRINT_DEBUG("marked_deleted = %u", target->marked_deleted);
  RBOX_PRINT_DEBUG("inbox_user = %u", target->inbox_user);
  RBOX_PRINT_DEBUG("inbox_any = %u", target->inbox_any);
  RBOX_PRINT_DEBUG("disable_reflink_copy_to = %u", target->disable_reflink_copy_to);
  RBOX_PRINT_DEBUG("disallow_new_keywords = %u", target->disallow_new_keywords);
  RBOX_PRINT_DEBUG("synced = %u", target->synced);
  RBOX_PRINT_DEBUG("mail_cache_disabled = %u", target->mail_cache_disabled);
  RBOX_PRINT_DEBUG("update_first_saved = %u", target->update_first_saved);
  RBOX_PRINT_DEBUG("skip_create_name_restrictions = %u", target->skip_create_name_restrictions);
  RBOX_PRINT_DEBUG("list_index_has_changed_quick = %u", target->list_index_has_changed_quick);
  RBOX_PRINT_DEBUG("corrupted_mailbox_name = %u", target->corrupted_mailbox_name);

  // debug_print_mail_storage(target->storage, "storage");
  RBOX_PRINT_DEBUG("storage = %p", target->storage);
  debug_print_mailbox_list(target->list, funcname, t_strdup_printf("  %s.%s", name, "list"));
  // RBOX_PRINT_DEBUG("list = %p", target->list);
  // debug_print_mail_index(target->index, "index");
  RBOX_PRINT_DEBUG("index = %p", target->index);
  // debug_print_mail_index(target->index_pvt, "index_pvt");
  RBOX_PRINT_DEBUG("index_pvt = %p", target->index_pvt);

  RBOX_PRINT_END()
}
void debug_print_rbox_mailbox(struct rbox_mailbox *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("rbox_mailbox")

  RBOX_PRINT_DEBUG("mailbox_guid = %s", guid_128_to_string(target->mailbox_guid));
  debug_print_mailbox(&target->box, funcname, t_strdup_printf("  %s.%s", name, "box"));

  RBOX_PRINT_END()
}

void debug_print_index_mail_data(struct index_mail_data *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("index_mail_data")

  RBOX_PRINT_DEBUG("date = %lu (%s)", target->date, unixdate2str(target->date));
  RBOX_PRINT_DEBUG("received_date = %lu (%s)", target->received_date, unixdate2str(target->received_date));
  RBOX_PRINT_DEBUG("save_date = %lu (%s)", target->save_date, unixdate2str(target->save_date));

  RBOX_PRINT_DEBUG("virtual_size = %lu", target->virtual_size);
  RBOX_PRINT_DEBUG("physical_size = %lu", target->physical_size);

  RBOX_PRINT_DEBUG("parts = %p", target->parts);
  RBOX_PRINT_DEBUG("bin_parts = %p", target->bin_parts);
  RBOX_PRINT_DEBUG("envelope_data = %p", target->envelope_data);
  RBOX_PRINT_DEBUG("wanted_headers = %p", target->wanted_headers);
  RBOX_PRINT_DEBUG("search_results = %p", target->search_results);
  RBOX_PRINT_DEBUG("stream = %p", target->stream);
  RBOX_PRINT_DEBUG("filter_stream = %p", target->filter_stream);
  RBOX_PRINT_DEBUG("tee_stream = %p", target->tee_stream);
  RBOX_PRINT_DEBUG("parser_input = %p", target->parser_input);
  RBOX_PRINT_DEBUG("parser_ctx = %p", target->parser_ctx);

  RBOX_PRINT_DEBUG("keywords size = %ld", target->keywords.arr.element_size);
  RBOX_PRINT_DEBUG("keyword_indexes size = %ld", target->keyword_indexes.arr.element_size);

  RBOX_PRINT_END()
}

void debug_print_mail_save_context(struct mail_save_context *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mail_save_context")

  RBOX_PRINT_DEBUG("unfinished = %u", target->unfinished);
  RBOX_PRINT_DEBUG("finishing = %u", target->finishing);
  RBOX_PRINT_DEBUG("copying_via_save = %u", target->copying_via_save);
  RBOX_PRINT_DEBUG("saving = %u", target->saving);
  RBOX_PRINT_DEBUG("moving = %u", target->moving);
  RBOX_PRINT_DEBUG("copying_or_moving = %u", target->copying_or_moving);
  RBOX_PRINT_DEBUG("dest_mail_external = %u", target->dest_mail_external);

  // debug_print_mail(target->dest_mail, NULL, "dest_mail");
  RBOX_PRINT_DEBUG("dest_mail = %p", target->dest_mail);
  if (target->dest_mail != NULL) {
    debug_print_mail(target->dest_mail, funcname, "dest_mail");
  }
  // debug_print_mail(target->copy_src_mail, NULL, "copy_src_mail");
  RBOX_PRINT_DEBUG("copy_src_mail = %p", target->copy_src_mail);
  if (target->copy_src_mail != NULL) {
    debug_print_mail(target->copy_src_mail, funcname, "copy_src_mail");
  }
  // debug_print_mailbox_transaction_context(mailSaveContext->transaction, NULL, "transaction");
  RBOX_PRINT_DEBUG("transaction = %p", target->transaction);
  // debug_print_mail_save_data(&mailSaveContext->data, NULL, "data");
  RBOX_PRINT_DEBUG("data = %p", &target->data);
  /*
    if (target->data != NULL) {
      RBOX_PRINT_DEBUG("data.uid = %u", target->data.uid);
      RBOX_PRINT_DEBUG("data.guid = %s", target->data.guid);
    }
  */

  RBOX_PRINT_END()
}

void debug_print_mailbox_transaction_context(struct mailbox_transaction_context *target, const char *funcname,
                                             const char *name) {
  RBOX_PRINT_START("mailbox_transaction_context")

  RBOX_PRINT_DEBUG("flags = 0x%04x", target->flags);
  RBOX_PRINT_DEBUG("mail_ref_count = %d", target->mail_ref_count);
  RBOX_PRINT_DEBUG("prev_pop3_uidl_tracking_seq = %u", target->prev_pop3_uidl_tracking_seq);
  RBOX_PRINT_DEBUG("highest_pop3_uidl_uid = %u", target->highest_pop3_uidl_uid);
  RBOX_PRINT_DEBUG("save_count = %u", target->save_count);
  RBOX_PRINT_DEBUG("stats_track = %u", target->stats_track);
  RBOX_PRINT_DEBUG("nontransactional_changes = %u", target->nontransactional_changes);
  RBOX_PRINT_DEBUG("internal_attribute = %d", target->internal_attribute);

  RBOX_PRINT_DEBUG("itrans = %p", target->itrans);
  RBOX_PRINT_DEBUG("attr_pvt_trans = %p", target->attr_pvt_trans);
  RBOX_PRINT_DEBUG("attr_shared_trans = %p", target->attr_shared_trans);
  RBOX_PRINT_DEBUG("view = %p", target->view);
  RBOX_PRINT_DEBUG("itrans_pvt = %p", target->itrans_pvt);
  RBOX_PRINT_DEBUG("view_pvt = %p", target->view_pvt);
  RBOX_PRINT_DEBUG("cache_view = %p", target->cache_view);
  RBOX_PRINT_DEBUG("cache_trans = %p", target->cache_trans);
  RBOX_PRINT_DEBUG("changes = %p", target->changes);
  RBOX_PRINT_DEBUG("stats.cache_hit_count = %lu", target->stats.cache_hit_count);
  RBOX_PRINT_DEBUG("module_contexts size = %ld", target->module_contexts.arr.element_size);
  RBOX_PRINT_DEBUG("pvt_saves size = %ld", target->pvt_saves.arr.element_size);

  // debug_print_mailbox(mailboxTransactionContext->box, NULL, "box");
  RBOX_PRINT_DEBUG("box = %p", target->box);
  // debug_print_mail_save_context(target->save_ctx, NULL, "save_ctx");
  RBOX_PRINT_DEBUG("save_ctx = %p", target->save_ctx);

  RBOX_PRINT_END()
}

void debug_print_mail_save_data(struct mail_save_data *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mail_save_data")

  RBOX_PRINT_DEBUG("flags = 0x%04x", target->flags);
  RBOX_PRINT_DEBUG("pvt_flags = 0x%04x", target->pvt_flags);
  RBOX_PRINT_DEBUG("min_modseq = %lu", target->min_modseq);
  RBOX_PRINT_DEBUG("received_date = %lu (%s)", target->received_date, unixdate2str(target->received_date));
  RBOX_PRINT_DEBUG("save_date = %lu (%s)", target->save_date, unixdate2str(target->save_date));
  RBOX_PRINT_DEBUG("received_tz_offset = %d", target->received_tz_offset);
  RBOX_PRINT_DEBUG("guid = %s", target->guid);
  RBOX_PRINT_DEBUG("pop3_uidl = %s", target->pop3_uidl);
  RBOX_PRINT_DEBUG("from_envelope = %s", target->from_envelope);
  RBOX_PRINT_DEBUG("pop3_order = %u", target->pop3_order);

  RBOX_PRINT_DEBUG("attach = %p", target->attach);
  RBOX_PRINT_DEBUG("keywords = %p", target->keywords);
  RBOX_PRINT_DEBUG("output = %p", target->output);

  RBOX_PRINT_END()
}

void debug_print_mail_storage(struct mail_storage *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mail_storage")

  RBOX_PRINT_DEBUG("name = %s", target->name);
  RBOX_PRINT_DEBUG("flags = 0x%04x", target->flags);
  RBOX_PRINT_DEBUG("class_flags = 0x%04x", target->class_flags);
  if (target->pool != NULL) {
    RBOX_PRINT_DEBUG("pool name = %s", target->pool->v->get_name(target->pool));
  }
  RBOX_PRINT_DEBUG("refcount = %d", target->refcount);
  RBOX_PRINT_DEBUG("obj_refcount = %d", target->obj_refcount);
  RBOX_PRINT_DEBUG("unique_root_dir = %s", target->unique_root_dir);
  RBOX_PRINT_DEBUG("error_string = %s", target->error_string);
  RBOX_PRINT_DEBUG("error = %s", enum_mail_error_strs[target->error]);
  RBOX_PRINT_DEBUG("temp_path_prefix = %s", target->temp_path_prefix);
  RBOX_PRINT_DEBUG("shared_attr_dict_failed = %u", target->shared_attr_dict_failed);

  RBOX_PRINT_DEBUG("prev = %p", target->prev);
  RBOX_PRINT_DEBUG("next = %p", target->next);
  RBOX_PRINT_DEBUG("mailboxes = %p", target->mailboxes);
  RBOX_PRINT_DEBUG("storage_class = %p", target->storage_class);
  RBOX_PRINT_DEBUG("set = %p", target->set);
  RBOX_PRINT_DEBUG("callback_context = %p", target->callback_context);
  RBOX_PRINT_DEBUG("_shared_attr_dict = %p", target->_shared_attr_dict);

  RBOX_PRINT_DEBUG("error_stack size = %ld", target->error_stack.arr.element_size);
  RBOX_PRINT_DEBUG("module_contexts size = %ld", target->module_contexts.arr.element_size);

  // debug_print_mail_user(mailStorage->user, NULL, "user");
  RBOX_PRINT_DEBUG("user = %p", target->user);

  RBOX_PRINT_END()
}

void debug_print_mail_user(struct mail_user *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mail_user")

  if (target->pool != NULL) {
    RBOX_PRINT_DEBUG("pool name = %s", target->pool->v->get_name(target->pool));
  }
  RBOX_PRINT_DEBUG("refcount = %d", target->refcount);
  RBOX_PRINT_DEBUG("username = %s", target->username);
  RBOX_PRINT_DEBUG("_home = %s", target->_home);
  RBOX_PRINT_DEBUG("uid = %u", target->uid);
  RBOX_PRINT_DEBUG("gid = %u", target->gid);
  RBOX_PRINT_DEBUG("service = %s", target->service);
  RBOX_PRINT_DEBUG("session_id = %s", target->session_id);
  RBOX_PRINT_DEBUG("auth_token = %s", target->auth_token);
  RBOX_PRINT_DEBUG("auth_user = %s", target->auth_user);
  RBOX_PRINT_DEBUG("userdb_fields = %s", target->userdb_fields != NULL ? *target->userdb_fields : "NULL");
  RBOX_PRINT_DEBUG("error = %s", target->error);
  RBOX_PRINT_DEBUG("session_create_time = %ld", target->session_create_time);

  RBOX_PRINT_DEBUG("creator = %p", target->creator);
  RBOX_PRINT_DEBUG("_service_user = %p", target->_service_user);
  RBOX_PRINT_DEBUG("local_ip = %p", target->local_ip);
  RBOX_PRINT_DEBUG("remote_ip = %p", target->remote_ip);
  RBOX_PRINT_DEBUG("var_expand_table = %p", target->var_expand_table);
  RBOX_PRINT_DEBUG("set_info = %p", target->set_info);
  RBOX_PRINT_DEBUG("unexpanded_set = %p", target->unexpanded_set);
  RBOX_PRINT_DEBUG("set = %p", target->set);
  RBOX_PRINT_DEBUG("namespaces = %p", target->namespaces);
  RBOX_PRINT_DEBUG("storages = %p", target->storages);
  RBOX_PRINT_DEBUG("default_normalizer = %p", target->default_normalizer);
  RBOX_PRINT_DEBUG("_attr_dict = %p", target->_attr_dict);

  RBOX_PRINT_DEBUG("hooks size = %ld", target->hooks.arr.element_size);
  RBOX_PRINT_DEBUG("module_contexts size = %ld", target->module_contexts.arr.element_size);

  RBOX_PRINT_DEBUG("nonexistent = %u", target->nonexistent);
  RBOX_PRINT_DEBUG("home_looked_up = %u", target->home_looked_up);
  RBOX_PRINT_DEBUG("anonymous = %u", target->anonymous);
  RBOX_PRINT_DEBUG("autocreated = %u", target->autocreated);
  RBOX_PRINT_DEBUG("initialized = %u", target->initialized);
  RBOX_PRINT_DEBUG("namespaces_created = %u", target->namespaces_created);
  RBOX_PRINT_DEBUG("settings_expanded = %u", target->settings_expanded);
  RBOX_PRINT_DEBUG("mail_debug = %u", target->mail_debug);
  RBOX_PRINT_DEBUG("inbox_open_error_logged = %u", target->inbox_open_error_logged);
  RBOX_PRINT_DEBUG("fuzzy_search = %u", target->fuzzy_search);
  RBOX_PRINT_DEBUG("dsyncing = %u", target->dsyncing);
  RBOX_PRINT_DEBUG("attr_dict_failed = %u", target->attr_dict_failed);
  RBOX_PRINT_DEBUG("deinitializing = %u", target->deinitializing);
  RBOX_PRINT_DEBUG("admin = %u", target->admin);
  RBOX_PRINT_DEBUG("stats_enabled = %u", target->stats_enabled);
  RBOX_PRINT_DEBUG("session_restored = %u", target->session_restored);

  RBOX_PRINT_END()
}

void debug_print_rbox_sync_context(struct rbox_sync_context *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("rbox_sync_context")

  RBOX_PRINT_DEBUG("path = %s", target->path != NULL ? (char *)target->path->data : "NULL");
  RBOX_PRINT_DEBUG("path_dir_prefix_len = %lu", target->path_dir_prefix_len);
  RBOX_PRINT_DEBUG("uid_validity = %u", target->uid_validity);

  RBOX_PRINT_DEBUG("index_sync_ctx = %p", target->index_sync_ctx);
  RBOX_PRINT_DEBUG("sync_view = %p", target->sync_view);
  RBOX_PRINT_DEBUG("trans = %p", target->trans);

  // debug_print_mailbox(&radosSyncContext->mbox->box, NULL, "mbox->box");
  // DEBUG("mbox->box._path = %s", radosSyncContext->mbox->box._path);

  RBOX_PRINT_END()
}

void debug_print_mailbox_list(struct mailbox_list *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mailbox_list")

  RBOX_PRINT_DEBUG("name = %s", target->name);
  RBOX_PRINT_DEBUG("props = 0x%04x", target->props);
  RBOX_PRINT_DEBUG("mailbox_name_max_length = %lu", target->mailbox_name_max_length);
  if (target->pool != NULL) {
    RBOX_PRINT_DEBUG("pool name = %s", target->pool->v->get_name(target->pool));
  }

  if (target->guid_cache_pool != NULL) {
    RBOX_PRINT_DEBUG("guid_cache_pool name = %s", target->guid_cache_pool->v->get_name(target->guid_cache_pool));
  }
  RBOX_PRINT_DEBUG("mail_set = %p", target->mail_set);
  RBOX_PRINT_DEBUG("subscriptions = %p", target->subscriptions);
  RBOX_PRINT_DEBUG("changelog = %p", target->changelog);

  RBOX_PRINT_DEBUG("root_permissions.file_uid = %u", target->root_permissions.file_uid);
  // RBOX_PRINT_DEBUG("module_contexts size = %ld", target->module_contexts.);

  RBOX_PRINT_DEBUG("subscriptions_mtime = %ld", target->subscriptions_mtime);
  RBOX_PRINT_DEBUG("subscriptions_read_time = %ld", target->subscriptions_read_time);
  RBOX_PRINT_DEBUG("changelog_timestamp = %ld", target->changelog_timestamp);
  RBOX_PRINT_DEBUG("guid_cache_pool = %p", target->guid_cache_pool);
  RBOX_PRINT_DEBUG("guid_cache_errors = %s", btoa(target->guid_cache_errors));
  RBOX_PRINT_DEBUG("error_string = %s", target->error_string);
  RBOX_PRINT_DEBUG("error = %s", enum_mail_error_strs[target->error]);
  RBOX_PRINT_DEBUG("temporary_error = %s", btoa(target->temporary_error));
  RBOX_PRINT_DEBUG("index_root_dir_created = %u", target->index_root_dir_created);
  RBOX_PRINT_DEBUG("guid_cache_updated = %u", target->guid_cache_updated);
  RBOX_PRINT_DEBUG("guid_cache_invalidated = %u", target->guid_cache_invalidated);

  // debug_print_%s_settings(&mailboxList->set, funcname, "set");
  RBOX_PRINT_DEBUG("set.inbox_path = %s", target->set.inbox_path);

  RBOX_PRINT_END()
}

void debug_print_mailbox_list_settings(struct mailbox_list_settings *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mailbox_list_settings")

  RBOX_PRINT_DEBUG("layout = %s", target->layout);
  RBOX_PRINT_DEBUG("root_dir = %s", target->root_dir);
  RBOX_PRINT_DEBUG("index_dir = %s", target->index_dir);
  RBOX_PRINT_DEBUG("index_pvt_dir = %s", target->index_pvt_dir);
  RBOX_PRINT_DEBUG("control_dir = %s", target->control_dir);
  RBOX_PRINT_DEBUG("alt_dir = %s", target->alt_dir);
  RBOX_PRINT_DEBUG("inbox_path = %s", target->inbox_path);
  RBOX_PRINT_DEBUG("subscription_fname = %s", target->subscription_fname);
  RBOX_PRINT_DEBUG("list_index_fname = %s", target->list_index_fname);
  RBOX_PRINT_DEBUG("maildir_name = %s", target->maildir_name);
  RBOX_PRINT_DEBUG("mailbox_dir_name = %s", target->mailbox_dir_name);
  RBOX_PRINT_DEBUG("escape_char = %c", target->escape_char);
  RBOX_PRINT_DEBUG("broken_char = %c", target->broken_char);
  RBOX_PRINT_DEBUG("utf8 = %s", btoa(target->utf8));
  RBOX_PRINT_DEBUG("alt_dir_nocheck = %s", btoa(target->alt_dir_nocheck));
  RBOX_PRINT_DEBUG("index_control_use_maildir_name = %s", btoa(target->index_control_use_maildir_name));

  RBOX_PRINT_END()
}

void debug_print_mail_index(struct mail_index *target, const char *funcname, const char *name) {
  RBOX_PRINT_START("mail_index")

  RBOX_PRINT_DEBUG("dir = %s", target->dir);
  RBOX_PRINT_DEBUG("prefix = %s", target->prefix);
  RBOX_PRINT_DEBUG("cache = %p", target->cache);
  RBOX_PRINT_DEBUG("log = %p", target->log);
  RBOX_PRINT_DEBUG("open_count = %u", target->open_count);
  RBOX_PRINT_DEBUG("flags = 0x%04x", target->flags);
  RBOX_PRINT_DEBUG("fsync_mode = 0x%04x", target->fsync_mode);
  RBOX_PRINT_DEBUG("fsync_mask = 0x%04x", target->fsync_mask);
  RBOX_PRINT_DEBUG("mode = %d", target->mode);
  RBOX_PRINT_DEBUG("gid = %d", target->gid);
  RBOX_PRINT_DEBUG("gid_origin = %s", target->gid_origin);
  RBOX_PRINT_DEBUG("log_rotate_min_size = %lu", target->log_rotate_min_size);
  RBOX_PRINT_DEBUG("log_rotate_max_size = %lu", target->log_rotate_max_size);
  RBOX_PRINT_DEBUG("log_rotate_min_created_ago_secs = %u", target->log_rotate_min_created_ago_secs);
  RBOX_PRINT_DEBUG("log_rotate_log2_stale_secs = %u", target->log_rotate_log2_stale_secs);
  RBOX_PRINT_DEBUG("extension_pool = %p", target->extension_pool);
  RBOX_PRINT_DEBUG("ext_hdr_init_id = %u", target->ext_hdr_init_id);
  RBOX_PRINT_DEBUG("ext_hdr_init_data = %p", target->ext_hdr_init_data);
  RBOX_PRINT_DEBUG("filepath = %s", target->filepath);
  RBOX_PRINT_DEBUG("fd = %d", target->fd);
  RBOX_PRINT_DEBUG("map = %p", target->map);
  RBOX_PRINT_DEBUG("last_mmap_error_time = %ld", target->last_mmap_error_time);
  RBOX_PRINT_DEBUG("indexid = %u", target->indexid);
  RBOX_PRINT_DEBUG("inconsistency_id = %u", target->inconsistency_id);
  RBOX_PRINT_DEBUG("last_read_log_file_seq = %u", target->last_read_log_file_seq);
  RBOX_PRINT_DEBUG("last_read_log_file_tail_offset = %u", target->last_read_log_file_tail_offset);
  RBOX_PRINT_DEBUG("fsck_log_head_file_seq = %u", target->fsck_log_head_file_seq);
  RBOX_PRINT_DEBUG("fsck_log_head_file_offset = %lu", target->fsck_log_head_file_offset);
  RBOX_PRINT_DEBUG("sync_commit_result = %p", target->sync_commit_result);
  RBOX_PRINT_DEBUG("lock_method = %s", enum_file_lock_method[target->lock_method]);
  RBOX_PRINT_DEBUG("max_lock_timeout_secs = %u", target->max_lock_timeout_secs);
  RBOX_PRINT_DEBUG("keywords_pool = %p", target->keywords_pool);
  RBOX_PRINT_DEBUG("keywords_ext_id = %u", target->keywords_ext_id);
  RBOX_PRINT_DEBUG("modseq_ext_id = %u", target->modseq_ext_id);
  RBOX_PRINT_DEBUG("views = %p", target->views);
  RBOX_PRINT_DEBUG("error = %s", target->error);
  RBOX_PRINT_DEBUG("nodiskspace = %u", target->nodiskspace);
  RBOX_PRINT_DEBUG("index_lock_timeout = %u", target->index_lock_timeout);
  RBOX_PRINT_DEBUG("index_delete_requested = %u", target->index_delete_requested);
  RBOX_PRINT_DEBUG("index_deleted = %u", target->index_deleted);
  RBOX_PRINT_DEBUG("log_sync_locked = %u", target->log_sync_locked);
  RBOX_PRINT_DEBUG("readonly = %u", target->readonly);
  RBOX_PRINT_DEBUG("mapping = %u", target->mapping);
  RBOX_PRINT_DEBUG("syncing = %u", target->syncing);
  RBOX_PRINT_DEBUG("need_recreate = %u", target->need_recreate);
  RBOX_PRINT_DEBUG("index_min_write = %u", target->index_min_write);
  RBOX_PRINT_DEBUG("modseqs_enabled = %u", target->modseqs_enabled);
  RBOX_PRINT_DEBUG("initial_create = %u", target->initial_create);
  RBOX_PRINT_DEBUG("initial_mapped = %u", target->initial_mapped);
  RBOX_PRINT_DEBUG("fscked = %u", target->fscked);

  RBOX_PRINT_DEBUG("cache = %p", target->cache);
  RBOX_PRINT_DEBUG("log = %p", target->log);
  RBOX_PRINT_DEBUG("ext_hdr_init_data = %p", target->ext_hdr_init_data);
  RBOX_PRINT_DEBUG("map = %p", target->map);
  RBOX_PRINT_DEBUG("sync_commit_result = %p", target->sync_commit_result);
  RBOX_PRINT_DEBUG("views = %p", target->views);
  RBOX_PRINT_DEBUG("extension_pool = %p", target->extension_pool);
  RBOX_PRINT_DEBUG("keywords_pool = %p", target->keywords_pool);

  RBOX_PRINT_DEBUG("extensions size = %ld", target->extensions.arr.element_size);
  RBOX_PRINT_DEBUG("sync_lost_handlers size = %ld", target->sync_lost_handlers.arr.element_size);
  RBOX_PRINT_DEBUG("keywords size = %ld", target->keywords.arr.element_size);
  RBOX_PRINT_DEBUG("module_contexts size = %ld", target->module_contexts.arr.element_size);

  RBOX_PRINT_END()
}
