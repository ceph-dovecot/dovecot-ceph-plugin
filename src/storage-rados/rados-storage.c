/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "mail-copy.h"
#include "index-mail.h"
#include "mailbox-list-private.h"
#include "rados-sync.h"
#include "rados-storage.h"
#include "debug-helper.h"

#include <sys/stat.h>

extern struct mail_storage rados_storage;
extern struct mailbox rados_mailbox;

static struct mail_storage *rados_storage_alloc(void)
{
	FUNC_START();
	struct rados_storage *storage;
	pool_t pool;

	pool = pool_alloconly_create("rados storage", 512+256);
	storage = p_new(pool, struct rados_storage, 1);
	storage->storage = rados_storage;
	storage->storage.pool = pool;
	debug_print_mail_storage(&storage->storage, "rados-storage::rados_storage_alloc", NULL);
	FUNC_END();
	return &storage->storage;
}

static void
rados_storage_get_list_settings(const struct mail_namespace *ns ATTR_UNUSED,
				struct mailbox_list_settings *set)
{
	FUNC_START();
	if (set->layout == NULL)
		set->layout = MAILBOX_LIST_NAME_INDEX;
	if (set->subscription_fname == NULL)
		set->subscription_fname = RADOS_SUBSCRIPTION_FILE_NAME;
	debug_print_mailbox_list_settings(set, "rados-storage::rados_storage_get_list_settings", NULL);
	FUNC_END();
}

static struct mailbox *
rados_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list,
		    const char *vname, enum mailbox_flags flags)
{
	FUNC_START();
	struct rados_mailbox *mbox;
	pool_t pool;

	/* rados can't work without index files */
	flags &= ~MAILBOX_FLAG_NO_INDEX_FILES;

	if (storage->set != NULL) {
		i_debug("mailbox_list_index = %s", btoa(storage->set->mailbox_list_index));
	}

	pool = pool_alloconly_create("rados mailbox", 1024*3);
	mbox = p_new(pool, struct rados_mailbox, 1);
	mbox->box = rados_mailbox;
	mbox->box.pool = pool;
	mbox->box.storage = storage;
	mbox->box.list = list;
	mbox->box.mail_vfuncs = &rados_mail_vfuncs;

	index_storage_mailbox_alloc(&mbox->box, vname, flags, MAIL_INDEX_PREFIX);

	mbox->storage = (struct rados_storage *)storage;
	i_debug("list name = %s", list->name);
	debug_print_mailbox(&mbox->box, "rados-storage::rados_mailbox_alloc", NULL);
	FUNC_END();
	return &mbox->box;
}

static int rados_mailbox_open(struct mailbox *box)
{
	FUNC_START();
	const char *box_path = mailbox_get_path(box);
	struct stat st;

	if (stat(box_path, &st) == 0) {
		/* exists, open it */
	} else if (errno == ENOENT) {
		mail_storage_set_error(box->storage, MAIL_ERROR_NOTFOUND,
			T_MAIL_ERR_MAILBOX_NOT_FOUND(box->vname));
		debug_print_mailbox(box, "rados-storage::rados_mailbox_open (ret -1, 1)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	} else if (errno == EACCES) {
		mail_storage_set_critical(box->storage, "%s",
			mail_error_eacces_msg("stat", box_path));
		debug_print_mailbox(box, "rados-storage::rados_mailbox_open (ret -1, 2)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	} else {
		mail_storage_set_critical(box->storage, "stat(%s) failed: %m",
					  box_path);
		debug_print_mailbox(box, "rados-storage::rados_mailbox_open (ret -1, 3)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	}
	if (index_storage_mailbox_open(box, FALSE) < 0) {
		debug_print_mailbox(box, "rados-storage::rados_mailbox_open (ret -1, 4)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	}
	mail_index_set_fsync_mode(box->index,
				  box->storage->set->parsed_fsync_mode,
				  MAIL_INDEX_FSYNC_MASK_APPENDS |
				  MAIL_INDEX_FSYNC_MASK_EXPUNGES);
	debug_print_mailbox(box, "rados-storage::rados_mailbox_open", NULL);
	FUNC_END();
	return 0;
}

static int
rados_mailbox_create(struct mailbox *box, const struct mailbox_update *update,
		     bool directory)
{
	FUNC_START();
	struct rados_mailbox *mbox = (struct rados_mailbox *)box;
	int ret;

	if ((ret = index_storage_mailbox_create(box, directory)) <= 0) {
		debug_print_mailbox(box, "rados-storage::rados_mailbox_create (ret <= 0, 1)", NULL);
		FUNC_END_RET("ret < 0");
		return ret;
	}

	ret = update == NULL ? 0 :
		index_storage_mailbox_update(box, update);

	i_debug("mailbox update = %p", update);
	debug_print_mailbox(box, "rados-storage::rados_mailbox_create", NULL);
	FUNC_END();
	return ret;
}


static int
rados_mailbox_get_metadata(struct mailbox *box,
			   enum mailbox_metadata_items items,
			   struct mailbox_metadata *metadata_r)
{
	FUNC_START();
	struct rados_mailbox *mbox = (struct rados_mailbox *)box;

	if ((items & MAILBOX_METADATA_GUID) != 0) {
		/* a bit ugly way to do this, but better than nothing for now.
		   FIXME: if indexes are enabled, keep this there. */
		mail_generate_guid_128_hash(box->name, metadata_r->guid);
		items &= ~MAILBOX_METADATA_GUID;
	}

	if (items != 0) {
		if (index_mailbox_get_metadata(box, items, metadata_r) < 0) {
			debug_print_mailbox(box, "rados-storage::rados_mailbox_get_metadata (ret -1, 1)", NULL);
			FUNC_END_RET("ret == -1");
			return -1;
		}
	}

	if (metadata_r != NULL && metadata_r->cache_fields != NULL) {
		i_debug("metadata size = %lu", metadata_r->cache_fields->arr.element_size);
	}
	debug_print_mailbox(box, "rados-storage::rados_mailbox_get_metadata", NULL);
	FUNC_END();
	return 0;
}

static void rados_notify_changes(struct mailbox *box)
{
	FUNC_START();
	if (box->notify_callback == NULL)
		mailbox_watch_remove_all(box);
	else
		mailbox_watch_add(box, mailbox_get_path(box));
	debug_print_mailbox(box, "rados-storage::rados_notify_changes", NULL);
	FUNC_END();
}

struct mail_storage rados_storage = {
	.name = RADOS_STORAGE_NAME,
	.class_flags = MAIL_STORAGE_CLASS_FLAG_FILE_PER_MSG |
		MAIL_STORAGE_CLASS_FLAG_BINARY_DATA,

	.v = {
		NULL,
		rados_storage_alloc,
		NULL,
		index_storage_destroy,
		NULL,
		rados_storage_get_list_settings,
		NULL,
		rados_mailbox_alloc,
		NULL,
		NULL,
	}
};

struct mailbox rados_mailbox = {
	.v = {
		index_storage_is_readonly,
		index_storage_mailbox_enable,
		index_storage_mailbox_exists,
		rados_mailbox_open,
		index_storage_mailbox_close,
		index_storage_mailbox_free,
		rados_mailbox_create,
		index_storage_mailbox_update,
		index_storage_mailbox_delete,
		index_storage_mailbox_rename,
		index_storage_get_status,
		rados_mailbox_get_metadata,
		index_storage_set_subscribed,
		index_storage_attribute_set,
		index_storage_attribute_get,
		index_storage_attribute_iter_init,
		index_storage_attribute_iter_next,
		index_storage_attribute_iter_deinit,
		index_storage_list_index_has_changed,
		index_storage_list_index_update_sync,
		rados_storage_sync_init,
		index_mailbox_sync_next,
		index_mailbox_sync_deinit,
		NULL,
		rados_notify_changes,
		index_transaction_begin,
		index_transaction_commit,
		index_transaction_rollback,
		NULL,
		index_mail_alloc,
		index_storage_search_init,
		index_storage_search_deinit,
		index_storage_search_next_nonblock,
		index_storage_search_next_update_seq,
		rados_save_alloc,
		rados_save_begin,
		rados_save_continue,
		rados_save_finish,
		rados_save_cancel,
		mail_storage_copy,
		rados_transaction_save_commit_pre,
		rados_transaction_save_commit_post,
		rados_transaction_save_rollback,
		index_storage_is_inconsistent
	}
};
