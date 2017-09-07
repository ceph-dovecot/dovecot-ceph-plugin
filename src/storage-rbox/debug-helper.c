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

#include <execinfo.h>
#include <time.h>

#include "dovecot-all.h"

#include "rbox-storage.hpp"
#include "rbox-sync.h"
#include "debug-helper.h"

// #define RBOX_DEBUG

#ifdef RBOX_DEBUG
static const char *enum_mail_access_type_strs[] = {"MAIL_ACCESS_TYPE_DEFAULT", "MAIL_ACCESS_TYPE_SEARCH",
                                                   "MAIL_ACCESS_TYPE_SORT"};
static const char *enum_mail_lookup_abort_strs[] = {"MAIL_LOOKUP_ABORT_NEVER", "MAIL_LOOKUP_ABORT_READ_MAIL",
                                                    "MAIL_LOOKUP_ABORT_NOT_IN_CACHE"};
static const char *enum_mail_error_strs[] = {
    "MAIL_ERROR_NONE",     "MAIL_ERROR_TEMP",          "MAIL_ERROR_NOTPOSSIBLE", "MAIL_ERROR_PARAMS",
    "MAIL_ERROR_PERM",     "MAIL_ERROR_NOQUOTA",       "MAIL_ERROR_NOTFOUND",    "MAIL_ERROR_EXISTS",
    "MAIL_ERROR_EXPUNGED", "MAIL_ERROR_INUSE",         "MAIL_ERROR_CONVERSION",  "MAIL_ERROR_INVALIDDATA",
    "MAIL_ERROR_LIMIT",    "MAIL_ERROR_LOOKUP_ABORTED"};
static const char *enum_file_lock_method[] = {"FILE_LOCK_METHOD_FCNTL", "FILE_LOCK_METHOD_FLOCK",
                                              "FILE_LOCK_METHOD_DOTLOCK"};
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

/* Obtain a backtrace and print it to stdout. */
void print_trace(void) {
  void *array[20];
  size_t size;
  char **strings;
  size_t i;

  size = backtrace(array, 20);
  strings = backtrace_symbols(array, size);

  i_debug(" ");
  for (i = 1; i < size; i++)
    i_debug("stack[%lu]: %s", i, strings[i]);
  i_debug(" ");

  free(strings);
}
