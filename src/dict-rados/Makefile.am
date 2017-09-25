#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	-I$(top_srcdir)/src/librmb

AUTOMAKE_OPTIONS = subdir-objects

LIBDICT_RADOS_PLUGIN = libdict_rados_plugin.la

shlibs = \
	$(top_builddir)/src/librmb/librmb.la

libdict_rados_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libdict_rados_plugin_la_LDFLAGS = -module -avoid-version
libdict_rados_plugin_la_LIBADD = $(LIBDOVECOT) $(shlibs)

module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBDICT_RADOS_PLUGIN)

libdict_rados_plugin_la_SOURCES = \
	libdict-rados-plugin.c \
	dict-rados.cpp \
	dovecot-dict.h \
	libdict-rados-plugin.h \
	dict-rados.h

