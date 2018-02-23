// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017-2018 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_STORAGE_RBOX_RBOX_COPY_H_
#define SRC_STORAGE_RBOX_RBOX_COPY_H_

int rbox_mail_copy(struct mail_save_context *_ctx, struct mail *mail);
bool rbox_is_op_on_shared_folder(struct mail *src_mail, struct mailbox *dest_mbox);

#endif /* SRC_STORAGE_RBOX_RBOX_COPY_H_ */
