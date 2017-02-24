AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE)

LIBRADOS_PLUGIN = librados_plugin.la
librados_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
librados_plugin_la_CXXFLAGS = -fpermissive
librados_plugin_la_LDFLAGS = -module -avoid-version
librados_plugin_la_LIBADD = $(LIBDOVECOT)


module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBRADOS_PLUGIN)

librados_plugin_la_SOURCES = \
 	librados-plugin.c \
    DictRados.cpp 

noinst_HEADERS = \
	DictRados.hpp \
	librados-plugin.h
	
test_programs = \
	test-rados 

test_rados_SOURCES = \
	test-rados.c \
	librados-plugin.c \
	DictRados.cpp 

test_rados_LDADD = $(LIBDOVECOT) $(LIBDOVECOT_STORAGE)

test_rados_DEPENDENCIES = $(LIBDOVECOT_DEPS)  $(LIBDOVECOT_STORAGE_DEPS)
test_rados_LDFLAGS = $(DOVECOT_BINARY_LDFLAGS)
test_rados_CFLAGS =  $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\" -fpermissive
test_rados_CXXFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\" -fpermissive

noinst_PROGRAMS = $(test_programs)
	