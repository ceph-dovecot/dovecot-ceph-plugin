AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE)

LIBRADOS_PLUGIN = librados_plugin.la
librados_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
librados_plugin_la_LDFLAGS = -module -avoid-version
librados_plugin_la_LIBADD = $(LIBDOVECOT)

module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBRADOS_PLUGIN)

librados_plugin_la_SOURCES = \
	librados-plugin.c

noinst_HEADERS = \
	librados-plugin.h
	
test_programs = \
	test-rados

test_rados_SOURCES = \
	test-rados.c \
	librados-plugin.c

test_rados_LDADD = $(LIBDOVECOT) $(LIBDOVECOT_STORAGE)
test_rados_DEPENDENCIES = $(LIBDOVECOT_DEPS)  $(LIBDOVECOT_STORAGE_DEPS)
test_rados_LDFLAGS = $(DOVECOT_BINARY_LDFLAGS)
test_rados_CFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\"

noinst_PROGRAMS = $(test_programs)

	