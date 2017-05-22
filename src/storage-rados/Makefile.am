AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE)

AM_CXXFLAGS = 

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
	rados-storage.c \
	debug-helper.c
	
noinst_HEADERS = \
	libstorage-rados-plugin.h \
	rados-storage.h \
	rados-sync.h \
	debug-helper.h
	
test_programs = \
	test-debug-helper

test_debug_helper_SOURCES = \
	test-debug-helper.c \
	debug-helper.c

test_debug_helper_LDADD = $(LIBDICT_RADOS_PLUGIN) $(LIBDOVECOT) $(LIBDOVECOT_STORAGE)
test_debug_helper_DEPENDENCIES = $(LIBDOVECOT_DEPS)  $(LIBDOVECOT_STORAGE_DEPS)
test_debug_helper_LDFLAGS = $(DOVECOT_BINARY_LDFLAGS)
test_debug_helper_CFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\"
test_debug_helper_CXXFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\" -fpermissive -std=c++11

noinst_PROGRAMS = $(test_programs)
	