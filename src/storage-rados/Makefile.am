AM_CPPFLAGS = \
	$(LIBDOVECOT_INCLUDE) \
	-I$(top_srcdir)/src/librmb

AM_CXXFLAGS = 

LIBSTORAGE_RADOS_PLUGIN = libstorage_rados_plugin.la

libstorage_rados_plugin_la_DEPENDENCIES = $(LIBDOVECOT_DEPS)
libstorage_rados_plugin_la_CFLAGS = 
libstorage_rados_plugin_la_CXXFLAGS = -fpermissive -std=c++11
libstorage_rados_plugin_la_LDFLAGS = -module -avoid-version
libstorage_rados_plugin_la_LIBADD = $(LIBDOVECOT) -lrmb

module_dir = $(moduledir)
module_LTLIBRARIES = \
	$(LIBSTORAGE_RADOS_PLUGIN)

libstorage_rados_plugin_la_SOURCES = \
	libstorage-rados-plugin.c \
	rados-mail.cpp \
	rados-save.cpp \
	rados-sync.cpp \
	rados-storage.cpp \
	debug-helper.c \
	RadosMailObject.cpp
	
	