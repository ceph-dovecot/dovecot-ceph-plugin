#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

AM_CPPFLAGS = \
	-I$(top_srcdir)/src/librmb

shlibs = \
	$(top_builddir)/src/librmb/librmb.la

bin_PROGRAMS = rmb

rmb_SOURCES = \
	mailbox_tools.cpp \
	mailbox_tools.h \
	ls_cmd_parser.cpp \
	ls_cmd_parser.h \
	rmb.cpp \
	rmb.h

rmb_LDADD = $(shlibs)

