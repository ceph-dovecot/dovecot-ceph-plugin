/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "mail-copy.h"
#include "index-mail.h"
#include "mail-index-modseq.h"
#include "mailbox-list-private.h"
#include "index-pop3-uidl.h"

#include "rados-sync.h"
#include "rados-storage.h"
#include "debug-helper.h"

#include <sys/stat.h>

extern struct mail_storage rados_storage;
extern struct mailbox rados_mailbox;

static struct mail_storage *rados_storage_alloc(void) {
	FUNC_START();
	struct rados_storage *storage;
	pool_t pool;

	pool = pool_alloconly_create("rados storage", 512 + 256);
	storage = p_new(pool, struct rados_storage, 1);
	storage->storage = rados_storage;
	storage->storage.pool = pool;
	debug_print_mail_storage(&storage->storage, "rados-storage::rados_storage_alloc", NULL);
	FUNC_END();
	return &storage->storage;
}

static void rados_storage_get_list_settings(const struct mail_namespace *ns ATTR_UNUSED, struct mailbox_list_settings *set) {
	FUNC_START();
	if (set->layout == NULL) {
		set->layout = MAILBOX_LIST_NAME_FS;
		// set->layout = MAILBOX_LIST_NAME_INDEX;
	}
	if (*set->maildir_name == '\0')
		set->maildir_name = RADOS_MAILDIR_NAME;
	if (*set->mailbox_dir_name == '\0')
		set->mailbox_dir_name = RADOS_MAILBOX_DIR_NAME;
	if (set->subscription_fname == NULL)
		set->subscription_fname = RADOS_SUBSCRIPTION_FILE_NAME;
	debug_print_mailbox_list_settings(set, "rados-storage::rados_storage_get_list_settings", NULL);
	FUNC_END();
}

static struct mailbox *
rados_mailbox_alloc(struct mail_storage *storage, struct mailbox_list *list, const char *vname, enum mailbox_flags flags) {
	FUNC_START();
	struct rados_mailbox *mbox;
	pool_t pool;

	/* rados can't work without index files */
	flags &= ~MAILBOX_FLAG_NO_INDEX_FILES;

	if (storage->set != NULL) {
		i_debug("mailbox_list_index = %s", btoa(storage->set->mailbox_list_index));
	}

	pool = pool_alloconly_create("rados mailbox", 1024 * 3);
	mbox = p_new(pool, struct rados_mailbox, 1);
	mbox->box = rados_mailbox;
	mbox->box.pool = pool;
	mbox->box.storage = storage;
	mbox->box.list = list;
	mbox->box.mail_vfuncs = &rados_mail_vfuncs;

	index_storage_mailbox_alloc(&mbox->box, vname, flags, MAIL_INDEX_PREFIX);

	mbox->storage = (struct rados_storage *) storage;
	i_debug("list name = %s", list->name);
	debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_alloc", NULL);
	FUNC_END();
	return &mbox->box;
}

static int rados_mailbox_alloc_index(struct rados_mailbox *mbox) {
	struct rbox_index_header hdr;

	if (index_storage_mailbox_alloc_index(&mbox->box) < 0)
		return -1;

	mbox->hdr_ext_id = mail_index_ext_register(mbox->box.index, "rados-hdr", sizeof(struct rbox_index_header), 0, 0);

	/* set the initialization data in case the mailbox is created */
	i_zero(&hdr);
	guid_128_generate(hdr.mailbox_guid);
	mail_index_set_ext_init_data(mbox->box.index, mbox->hdr_ext_id, &hdr, sizeof(hdr));

	debug_print_rbox_index_header(&hdr, "rados-storage::rados_mailbox_alloc_index", NULL);
	debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_alloc_index", NULL);

	return 0;
}

static int rados_read_header(struct rados_mailbox *mbox, struct rbox_index_header *hdr, bool log_error, bool *need_resize_r) {
	struct mail_index_view *view;
	const void *data;
	size_t data_size;
	int ret = 0;

	i_assert(mbox->box.opened);

	debug_print_rbox_index_header(&hdr, "rados-storage::rados_read_header", "in");

	view = mail_index_view_open(mbox->box.index);
	mail_index_get_header_ext(view, mbox->hdr_ext_id, &data, &data_size);
	if (data_size < RADOS_INDEX_HEADER_MIN_SIZE && (!mbox->box.creating || data_size != 0)) {
		if (log_error) {
			mail_storage_set_critical(&mbox->storage->storage, "rados %s: Invalid rados header size", mailbox_get_path(&mbox->box));
		}
		ret = -1;
	} else {
		i_zero(hdr);
		memcpy(hdr, data, I_MIN(data_size, sizeof(*hdr)));
		if (guid_128_is_empty(hdr->mailbox_guid))
			ret = -1;
		else {
			/* data is valid. remember it in case mailbox
			 is being reset */
			mail_index_set_ext_init_data(mbox->box.index, mbox->hdr_ext_id, hdr, sizeof(*hdr));
		}
	}
	mail_index_view_close(&view);
	*need_resize_r = data_size < sizeof(*hdr);

	debug_print_rbox_index_header(hdr, "rados-storage::rados_read_header", "out");

	return ret;
}

static void rados_update_header(struct rados_mailbox *mbox, struct mail_index_transaction *trans,
		const struct mailbox_update *update) {
	struct rbox_index_header hdr, new_hdr;
	bool need_resize;

	if (rados_read_header(mbox, &hdr, TRUE, &need_resize) < 0) {
		i_zero(&hdr);
		need_resize = TRUE;
	}

	new_hdr = hdr;

	if (update != NULL && !guid_128_is_empty(update->mailbox_guid)) {
		memcpy(new_hdr.mailbox_guid, update->mailbox_guid, sizeof(new_hdr.mailbox_guid));
	} else if (guid_128_is_empty(new_hdr.mailbox_guid)) {
		guid_128_generate(new_hdr.mailbox_guid);
	}

	if (need_resize) {
		mail_index_ext_resize_hdr(trans, mbox->hdr_ext_id, sizeof(new_hdr));
	}
	if (memcmp(&hdr, &new_hdr, sizeof(hdr)) != 0) {
		mail_index_update_header_ext(trans, mbox->hdr_ext_id, 0, &new_hdr, sizeof(new_hdr));
	}
	memcpy(mbox->mailbox_guid, new_hdr.mailbox_guid, sizeof(mbox->mailbox_guid));
}

static int rados_mailbox_create_indexes(struct mailbox *box, const struct mailbox_update *update,
		struct mail_index_transaction *trans) {
	struct rados_mailbox *mbox = (struct rados_mailbox *) box;
	struct mail_index_transaction *new_trans = NULL;
	const struct mail_index_header *hdr;
	uint32_t uid_validity, uid_next;

	if (trans == NULL) {
		new_trans = mail_index_transaction_begin(box->view, 0);
		trans = new_trans;
	}

	hdr = mail_index_get_header(box->view);
	if (update != NULL && update->uid_validity != 0)
		uid_validity = update->uid_validity;
	else if (hdr->uid_validity != 0)
		uid_validity = hdr->uid_validity;
	else {
		/* set uidvalidity */
		// TODO uid_validity = dbox_get_uidvalidity_next(box->list);
	}

	if (hdr->uid_validity != uid_validity) {
		mail_index_update_header(trans, offsetof(struct mail_index_header, uid_validity), &uid_validity, sizeof(uid_validity),
		TRUE);
	}
	if (update != NULL && hdr->next_uid < update->min_next_uid) {
		uid_next = update->min_next_uid;
		mail_index_update_header(trans, offsetof(struct mail_index_header, next_uid), &uid_next, sizeof(uid_next), TRUE);
	}
	if (update != NULL && update->min_first_recent_uid != 0 && hdr->first_recent_uid < update->min_first_recent_uid) {
		uint32_t first_recent_uid = update->min_first_recent_uid;

		mail_index_update_header(trans, offsetof(struct mail_index_header, first_recent_uid), &first_recent_uid,
				sizeof(first_recent_uid), FALSE);
	}
	if (update != NULL && update->min_highest_modseq != 0
			&& mail_index_modseq_get_highest(box->view) < update->min_highest_modseq) {
		mail_index_modseq_enable(box->index);
		mail_index_update_highest_modseq(trans, update->min_highest_modseq);
	}

	if (box->inbox_user && box->creating) {
		/* initialize pop3-uidl header when creating mailbox
		 (not on mailbox_update()) */
		index_pop3_uidl_set_max_uid(box, trans, 0);
	}

	rados_update_header(mbox, trans, update);
	if (new_trans != NULL) {
		if (mail_index_transaction_commit(&new_trans) < 0) {
			mailbox_set_index_error(box);
			return -1;
		}
	}
	return 0;
}

static int rados_mailbox_open(struct mailbox *box) {
	FUNC_START();
	struct rados_mailbox *mbox = (struct rados_mailbox *) box;
	struct rbox_index_header hdr;
	bool need_resize;

	if (rados_mailbox_alloc_index(mbox) < 0)
		return -1;

	const char *box_path = mailbox_get_path(box);
	struct stat st;

	i_debug("rados-storage::rados_mailbox_open box_path = %s", box_path);

	if (stat(box_path, &st) == 0) {
		/* exists, open it */
	} else if (errno == ENOENT) {
		mail_storage_set_error(box->storage, MAIL_ERROR_NOTFOUND, T_MAIL_ERR_MAILBOX_NOT_FOUND(box->vname));
		debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 1)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	} else if (errno == EACCES) {
		mail_storage_set_critical(box->storage, "%s", mail_error_eacces_msg("stat", box_path));
		debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 2)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	} else {
		mail_storage_set_critical(box->storage, "stat(%s) failed: %m", box_path);
		debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 3)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	}

	if (index_storage_mailbox_open(box, FALSE) < 0) {
		debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open (ret -1, 4)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	}

	mail_index_set_fsync_mode(box->index, box->storage->set->parsed_fsync_mode,
			MAIL_INDEX_FSYNC_MASK_APPENDS | MAIL_INDEX_FSYNC_MASK_EXPUNGES);

	/* get/generate mailbox guid */
	if (rados_read_header(mbox, &hdr, FALSE, &need_resize) < 0) {
		/* looks like the mailbox is corrupted */
		// (void) rados_sync(mbox, RADOS_SYNC_FLAG_FORCE);
		(void) rados_sync(mbox);
		if (rados_read_header(mbox, &hdr, TRUE, &need_resize) < 0)
			i_zero(&hdr);
	}

	if (guid_128_is_empty(hdr.mailbox_guid)) {
		/* regenerate it */
		if (rados_mailbox_create_indexes(box, NULL, NULL) < 0 || rados_read_header(mbox, &hdr, TRUE, &need_resize) < 0)
			return -1;
	}
	memcpy(mbox->mailbox_guid, hdr.mailbox_guid, sizeof(mbox->mailbox_guid));

	debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_open", NULL);
	FUNC_END();
	return 0;
}

static int rados_mailbox_create(struct mailbox *box, const struct mailbox_update *update, bool directory) {
	FUNC_START();
	struct rados_mailbox *mbox = (struct rados_mailbox *) box;
	int ret;

	if ((ret = index_storage_mailbox_create(box, directory)) <= 0) {
		debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_create (ret <= 0, 1)", NULL);
		FUNC_END_RET("ret < 0");
		return ret;
	}

	ret = update == NULL ? 0 : index_storage_mailbox_update(box, update);

	i_debug("mailbox update = %p", update);
	debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_create", NULL);
	FUNC_END();
	return ret;
}

static int rados_mailbox_get_metadata(struct mailbox *box, enum mailbox_metadata_items items, struct mailbox_metadata *metadata_r) {
	FUNC_START();
	struct rados_mailbox *mbox = (struct rados_mailbox *) box;

	if (index_mailbox_get_metadata(box, items, metadata_r) < 0) {
		debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_get_metadata (ret -1, 1)", NULL);
		FUNC_END_RET("ret == -1");
		return -1;
	}

	if ((items & MAILBOX_METADATA_GUID) != 0) {
		/* a bit ugly way to do this, but better than nothing for now.
		 FIXME: if indexes are enabled, keep this there. */
		mail_generate_guid_128_hash(box->name, metadata_r->guid);
		items &= ~MAILBOX_METADATA_GUID;
	}


#ifdef NEVER
	if ((items & MAILBOX_METADATA_GUID) != 0) {
		memcpy(metadata_r->guid, mbox->mailbox_guid, sizeof(metadata_r->guid));
	}
#endif

	if (metadata_r != NULL && metadata_r->cache_fields != NULL) {
		i_debug("metadata size = %lu", metadata_r->cache_fields->arr.element_size);
	}

	debug_print_rados_mailbox(mbox, "rados-storage::rados_mailbox_get_metadata", NULL);
	debug_print_mailbox_metadata(metadata_r, "rados-storage::rados_mailbox_get_metadata", NULL);

	FUNC_END();
	return 0;
}

static void rados_notify_changes(struct mailbox *box) {
	FUNC_START();
	struct rados_mailbox *mbox = (struct rados_mailbox *) box;

	if (box->notify_callback == NULL)
		mailbox_watch_remove_all(box);
	else
		mailbox_watch_add(box, mailbox_get_path(box));
	debug_print_rados_mailbox(mbox, "rados-storage::rados_notify_changes", NULL);
	FUNC_END();
}

struct mail_storage rados_storage = { .name = RADOS_STORAGE_NAME, .class_flags = MAIL_STORAGE_CLASS_FLAG_FILE_PER_MSG
		| MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_GUIDS | MAIL_STORAGE_CLASS_FLAG_HAVE_MAIL_SAVE_GUIDS
		| MAIL_STORAGE_CLASS_FLAG_BINARY_DATA,

.v = {
NULL, rados_storage_alloc,
NULL, index_storage_destroy,
NULL, rados_storage_get_list_settings,
NULL, rados_mailbox_alloc,
NULL,
NULL, } };

struct mailbox rados_mailbox = { .v = {
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
		index_storage_is_inconsistent } };
