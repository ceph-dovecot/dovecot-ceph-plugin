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
	
test_programs = \
	test-dict-rados

test_dict_rados_SOURCES = \
	test-dict-rados.c 

test_dict_rados_LDADD = $(LIBDICT_RADOS_PLUGIN) $(LIBDOVECOT) $(LIBDOVECOT_STORAGE)
test_dict_rados_DEPENDENCIES = $(LIBDICT_RADOS_PLUGIN) $(LIBDOVECOT_DEPS)  $(LIBDOVECOT_STORAGE_DEPS)
test_dict_rados_LDFLAGS = $(DOVECOT_BINARY_LDFLAGS) $(LIBDICT_RADOS_PLUGIN) $(shlibs)
test_dict_rados_CFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -fPIC

noinst_PROGRAMS = $(test_programs)

