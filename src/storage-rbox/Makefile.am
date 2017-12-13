#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

LIBSTORAGE_RBOX_PLUGIN = libstorage_rbox_plugin.la

shlibs = \
	$(top_builddir)/src/librmb/librmb.la

libstorage_rbox_plugin_la_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_STORAGE_INCLUDE) \
	-I$(top_srcdir)/src/librmb

libstorage_rbox_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libstorage_rbox_plugin_la_LDFLAGS = -module -avoid-version
libstorage_rbox_plugin_la_LIBADD = $(LIBDOVECOT) $(shlibs)

module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBSTORAGE_RBOX_PLUGIN)

libstorage_rbox_plugin_la_SOURCES = \
	libstorage-rbox-plugin.c \
	rbox-copy.cpp \
	rbox-mail.cpp \
	rbox-save.cpp \
	rbox-sync.cpp \
	rbox-storage.cpp \
	rbox-sync-rebuild.cpp \
	istream-bufferlist.cpp \
	debug-helper.c \
	debug-helper.h \
	dovecot-all.h \
	libstorage-rbox-plugin.h \
	rbox-copy.h \
	rbox-mail.h \
	rbox-save.h \
	rbox-storage-struct.h \
	rbox-storage.h \
	rbox-storage.hpp \
	rbox-sync-rebuild.h \
	rbox-sync.h \
	typeof-def.h \
	istream-bufferlist.h

