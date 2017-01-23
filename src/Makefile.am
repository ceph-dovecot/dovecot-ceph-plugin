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
	test-mail-key

test_mail_key_SOURCES = \
	test-rados.c \
	librados-plugin.c

test_mail_key_LDADD = $(LIBDOVECOT) $(LIBDOVECOT_STORAGE)
test_mail_key_DEPENDENCIES = $(LIBDOVECOT_DEPS)  $(LIBDOVECOT_STORAGE_DEPS)
test_mail_key_LDFLAGS = $(DOVECOT_BINARY_LDFLAGS)
test_mail_key_CFLAGS = $(AM_CPPFLAGS) $(DOVECOT_BINARY_CFLAGS) -Dtop_builddir=\"$(top_builddir)\"

noinst_PROGRAMS = $(test_programs)

	