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

#ifndef SRC_STORAGE_RBOX_TYPEOF_DEF_H_
#define SRC_STORAGE_RBOX_TYPEOF_DEF_H_

#include "module-context.h"
#ifdef HAVE_TYPEOF
#define typeof __typeof__
#endif

/* replacement for INDEX_STORAGE_CONTEXT (v 2.3, assigned void or long int, this is not allowed with c++) */
#define RBOX_INDEX_STORAGE_CONTEXT(obj) RBOX_MODULE_CONTEXT(obj, index_storage_module)

/* replacement for MODULE_CONTEXT (v 2.3, assigned void or long int, this is not allowed with c++) */
#define RBOX_MODULE_CONTEXT(obj, id_ctx)                                                                  \
  (module_get_context_id(&(id_ctx).id) < array_count(&(obj)->module_contexts)                             \
       ? (*((index_mailbox_context **)array_idx_modifiable(&(obj)->module_contexts, module_get_context_id(&(id_ctx).id)) + \
            OBJ_REGISTER_COMPATIBLE(obj, id_ctx)))                                                        \
       : NULL)

#endif  // SRC_STORAGE_RBOX_TYPEOF_DEF_H_
