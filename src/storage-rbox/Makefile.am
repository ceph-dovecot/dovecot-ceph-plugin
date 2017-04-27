AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE)

AM_CXXFLAGS = \
	$(LIBDOVECOT_INCLUDE)

LIBSTORAGE_RBOX_PLUGIN = libstorage_rbox_plugin.la

libstorage_rbox_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libstorage_rbox_plugin_la_CFLAGS = 
libstorage_rbox_plugin_la_CXXFLAGS = -fpermissive -std=c++11
libstorage_rbox_plugin_la_LDFLAGS = -module -avoid-version

libstorage_rbox_plugin_la_LIBADD = $(LIBDOVECOT)

module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBSTORAGE_RBOX_PLUGIN)

libstorage_rbox_plugin_la_SOURCES = \
	libstorage-rbox-plugin.c \
	rbox-copy.c \
	rbox-file.c \
	rbox-mail.c \
	rbox-save.c \
	rbox-sync.c \
	rbox-sync-rebuild.c \
	rbox-storage.c \
	debug-helper.c
	
noinst_HEADERS = \
	libstorage-rbox-plugin.h \
	rbox-file.h \
	rbox-storage.h \
	rbox-sync.h \
	debug-helper.h
	
test_programs = \
	test-debug-helper

test_debug_helper_SOURCES = \
	test-debug-helper.c \
	debug-helper.c

test_debug_helper_LDADD = $(LIBDICT_RBOX_PLUGIN) $(LIBDOVECOT) $(LIBDOVECOT_STORAGE)
test_debug_helper_DEPENDENCIES = $(LIBDOVECOT_DEPS)  $(LIBDOVECOT_STORAGE_DEPS)
test_debug_helper_LDFLAGS = $(DOVECOT_BINARY_LDFLAGS)
test_debug_helper_CFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\"
test_debug_helper_CXXFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\" -fpermissive -std=c++11

noinst_PROGRAMS = $(test_programs)
	