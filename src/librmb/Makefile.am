	
AM_CPPFLAGS = 

AM_CXXFLAGS = 

# noinst_LTLIBRARIES = librmb.la

module_LTLIBRARIES = \
	librmb.la

librmb_la_SOURCES = \
	rados-cluster.cpp \
	rados-storage.cpp \
	rados-dictionary.cpp \
	rados-mail-object.cpp 

librmb_la_CFLAGS = 
librmb_la_CXXFLAGS = -fpermissive -std=c++11

headers = \
	rados-cluster.h \
	rados-storage.h \
	rados-dictionary.h \
	rados-mail-object.h

pkginc_libdir=$(pkgincludedir)
pkginc_lib_HEADERS = $(headers)
	