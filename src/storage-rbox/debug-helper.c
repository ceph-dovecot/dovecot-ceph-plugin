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

#include <execinfo.h>
#include <time.h>

#include "dovecot-all.h"

#include "debug-helper.h"

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
