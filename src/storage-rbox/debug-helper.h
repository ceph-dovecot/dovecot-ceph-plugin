// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*-
// vim: ts=8 sw=2 smarttab
/*
 * Copyright (c) 2017 Tallence AG and the authors
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 */

#ifndef SRC_STORAGE_RBOX_DEBUG_HELPER_H_
#define SRC_STORAGE_RBOX_DEBUG_HELPER_H_

struct rbox_sync_context;
struct index_mail_data;

#define btoa(x) ((x) ? "true" : "false")

#ifdef NDEBUG
#define FUNC_START() ((void)0)
#define FUNC_END() ((void)0)
#define FUNC_END_RET(ignore) ((void)0)
#define FUNC_END_RET_INT(ignore) ((void)0)
#else
#define FUNC_START() i_debug("[START] %s: %s at line %d", __FILE__, __func__, __LINE__)
#define FUNC_END() i_debug("[END] %s: %s at line %d\n", __FILE__, __func__, __LINE__)
#define FUNC_END_RET(ret) i_debug("[END] %s: %s at line %d, %s\n", __FILE__, __func__, __LINE__, ret)
#define FUNC_END_RET_INT(ret) i_debug("[END] %s: %s at line %d, ret==%d\n", __FILE__, __func__, __LINE__, ret)
#endif

// Can be used to output a stacktrace
void print_trace(void);
// Convert unix date to string
const char *unixdate2str(time_t timestamp);

#endif  // SRC_STORAGE_RBOX_DEBUG_HELPER_H_
