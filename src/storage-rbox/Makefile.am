AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	$(LIBDOVECOT_STORAGE_INCLUDE) \
	-I$(top_srcdir)/src/librmb

AM_CXXFLAGS = 

LIBSTORAGE_RBOX_PLUGIN = libstorage_rbox_plugin.la

shlibs = \
	../librmb/librmb.la

libstorage_rbox_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libstorage_rbox_plugin_la_CFLAGS = 
libstorage_rbox_plugin_la_CXXFLAGS = -fpermissive -std=c++11
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
	debug-helper.c 
	
