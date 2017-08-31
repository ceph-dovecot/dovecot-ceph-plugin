# Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file

AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	-I$(top_srcdir)/src/librmb

AUTOMAKE_OPTIONS = subdir-objects

LIBDICT_RADOS_PLUGIN = libdict_rados_plugin.la

shlibs = \
	$(top_srcdir)/src/librmb/librmb.la

libdict_rados_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libdict_rados_plugin_la_LDFLAGS = -module -avoid-version
libdict_rados_plugin_la_LIBADD = $(LIBDOVECOT) $(shlibs)

module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBDICT_RADOS_PLUGIN)

libdict_rados_plugin_la_SOURCES = \
	libdict-rados-plugin.c \
    dict-rados.cpp 

noinst_HEADERS = \
	dict-rados.h \
	libdict-rados-plugin.h
