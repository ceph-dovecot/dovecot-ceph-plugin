AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE)

AM_CXXFLAGS = \
	$(LIBDOVECOT_INCLUDE)

LIBSTORAGE_RADOS_PLUGIN = libstorage_rados_plugin.la

libstorage_rados_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libstorage_rados_plugin_la_CFLAGS = 
libstorage_rados_plugin_la_CXXFLAGS = -fpermissive -std=c++11
libstorage_rados_plugin_la_LDFLAGS = -module -avoid-version

libstorage_rados_plugin_la_LIBADD = $(LIBDOVECOT)

module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBSTORAGE_RADOS_PLUGIN)

libstorage_rados_plugin_la_SOURCES = \
	libstorage-rados-plugin.c \
	rados-mail.c \
	rados-save.c \
	rados-sync.c \
	rados-storage.c
	
noinst_HEADERS = \
	libstorage-rados-plugin.h \
	rados-storage.h \
	rados-sync.h
	
