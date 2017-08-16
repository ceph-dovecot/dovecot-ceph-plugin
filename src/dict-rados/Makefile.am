AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	-I$(top_srcdir)/src/librmb

AM_CXXFLAGS = 

AUTOMAKE_OPTIONS = subdir-objects

LIBDICT_RADOS_PLUGIN = libdict_rados_plugin.la

shlibs = \
	$(top_srcdir)/src/librmb/librmb.la

libdict_rados_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libdict_rados_plugin_la_CFLAGS = 
libdict_rados_plugin_la_CXXFLAGS = -fpermissive -std=c++11
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

test_libs = \
	$(shlibs)

test_dict_rados_SOURCES = \
	test-dict-rados.c \
	libdict-rados-plugin.c \
    dict-rados.cpp 

test_dict_rados_LDADD = $(LIBDICT_RADOS_PLUGIN) $(LIBDOVECOT) $(LIBDOVECOT_STORAGE)
test_dict_rados_DEPENDENCIES = $(test_libs) $(LIBDOVECOT_DEPS)  $(LIBDOVECOT_STORAGE_DEPS)
test_dict_rados_LDFLAGS = $(DOVECOT_BINARY_LDFLAGS) $(test_libs)
test_dict_rados_CFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\"
test_dict_rados_CXXFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\" -fpermissive -std=c++11

noinst_PROGRAMS = $(test_programs)

