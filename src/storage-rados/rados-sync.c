/* Copyright (c) 2007-2017 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "ioloop.h"
#include "str.h"
#include "rados-storage.h"
#include "rados-sync.h"

static void rados_sync_set_uidvalidity(struct rados_sync_context *ctx)
{
	uint32_t uid_validity = ioloop_time;

	mail_index_update_header(ctx->trans,
		offsetof(struct mail_index_header, uid_validity),
		&uid_validity, sizeof(uid_validity), TRUE);
	ctx->uid_validity = uid_validity;
}

static string_t *rados_get_path_prefix(struct rados_mailbox *mbox)
{
	string_t *path = str_new(default_pool, 256);

	str_append(path, mailbox_get_path(&mbox->box));
	str_append_c(path, '/');
	return path;
}

static void
rados_sync_expunge(struct rados_sync_context *ctx, uint32_t seq1, uint32_t seq2)
{
	struct mailbox *box = &ctx->mbox->box;
	uint32_t uid;

	if (ctx->path == NULL) {
		ctx->path = rados_get_path_prefix(ctx->mbox);
		ctx->path_dir_prefix_len = str_len(ctx->path);
	}

	for (; seq1 <= seq2; seq1++) {
		mail_index_lookup_uid(ctx->sync_view, seq1, &uid);

		str_truncate(ctx->path, ctx->path_dir_prefix_len);
		str_printfa(ctx->path, "%u.", uid);
		if (i_unlink_if_exists(str_c(ctx->path)) < 0) {
			/* continue anyway */
		} else {
			if (box->v.sync_notify != NULL) {
				box->v.sync_notify(box, uid,
						   MAILBOX_SYNC_TYPE_EXPUNGE);
			}
			mail_index_expunge(ctx->trans, seq1);
		}
	}
}

static void rados_sync_index(struct rados_sync_context *ctx)
{
	struct mailbox *box = &ctx->mbox->box;
	const struct mail_index_header *hdr;
	struct mail_index_sync_rec sync_rec;
	uint32_t seq1, seq2;

	hdr = mail_index_get_header(ctx->sync_view);
	if (hdr->uid_validity != 0)
		ctx->uid_validity = hdr->uid_validity;
	else
		rados_sync_set_uidvalidity(ctx);

	/* mark the newly seen messages as recent */
	if (mail_index_lookup_seq_range(ctx->sync_view, hdr->first_recent_uid,
					hdr->next_uid, &seq1, &seq2)) {
		mailbox_recent_flags_set_seqs(&ctx->mbox->box, ctx->sync_view,
					      seq1, seq2);
	}

	while (mail_index_sync_next(ctx->index_sync_ctx, &sync_rec)) {
		if (!mail_index_lookup_seq_range(ctx->sync_view,
						 sync_rec.uid1, sync_rec.uid2,
						 &seq1, &seq2)) {
			/* already expunged, nothing to do. */
			continue;
		}

		switch (sync_rec.type) {
		case MAIL_INDEX_SYNC_TYPE_EXPUNGE:
			rados_sync_expunge(ctx, seq1, seq2);
			break;
		case MAIL_INDEX_SYNC_TYPE_FLAGS:
		case MAIL_INDEX_SYNC_TYPE_KEYWORD_ADD:
		case MAIL_INDEX_SYNC_TYPE_KEYWORD_REMOVE:
			/* FIXME: should be bother calling sync_notify()? */
			break;
		}
	}

	if (box->v.sync_notify != NULL)
		box->v.sync_notify(box, 0, 0);
}

int rados_sync_begin(struct rados_mailbox *mbox,
		     struct rados_sync_context **ctx_r, bool force)
{
	struct rados_sync_context *ctx;
	enum mail_index_sync_flags sync_flags;
	int ret;

	ctx = i_new(struct rados_sync_context, 1);
	ctx->mbox = mbox;

	sync_flags = index_storage_get_sync_flags(&mbox->box) |
		MAIL_INDEX_SYNC_FLAG_FLUSH_DIRTY;
	if (!force)
		sync_flags |= MAIL_INDEX_SYNC_FLAG_REQUIRE_CHANGES;

	ret = index_storage_expunged_sync_begin(&mbox->box, &ctx->index_sync_ctx,
						&ctx->sync_view, &ctx->trans,
						sync_flags);
	if (ret <= 0) {
		i_free(ctx);
		*ctx_r = NULL;
		return ret;
	}

	rados_sync_index(ctx);
	index_storage_expunging_deinit(&mbox->box);

	*ctx_r = ctx;
	return 0;
}

int rados_sync_finish(struct rados_sync_context **_ctx, bool success)
{
	struct rados_sync_context *ctx = *_ctx;
	int ret = success ? 0 : -1;

	*_ctx = NULL;
	if (success) {
		if (mail_index_sync_commit(&ctx->index_sync_ctx) < 0) {
			mailbox_set_index_error(&ctx->mbox->box);
			ret = -1;
		}
	} else {
		mail_index_sync_rollback(&ctx->index_sync_ctx);
	}
	if (ctx->path != NULL)
		str_free(&ctx->path);
	i_free(ctx);
	return ret;
}

static int rados_sync(struct rados_mailbox *mbox)
{
	struct rados_sync_context *sync_ctx;

	if (rados_sync_begin(mbox, &sync_ctx, FALSE) < 0)
		return -1;

	return sync_ctx == NULL ? 0 :
		rados_sync_finish(&sync_ctx, TRUE);
}

struct mailbox_sync_context *
rados_storage_sync_init(struct mailbox *box, enum mailbox_sync_flags flags)
{
	struct rados_mailbox *mbox = (struct rados_mailbox *)box;
	int ret = 0;

	if (!box->opened) {
		if (mailbox_open(box) < 0)
			ret = -1;
	}

	if (index_mailbox_want_full_sync(&mbox->box, flags) && ret == 0)
		ret = rados_sync(mbox);

	return index_mailbox_sync_init(box, flags, ret < 0);
}
