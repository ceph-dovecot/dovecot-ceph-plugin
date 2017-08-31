# Copyright (c) 2017 Tallence AG and the authors, see the included COPYING file

SUBDIRS = . tools
     
lib_LTLIBRARIES = \
	librmb.la

librmb_la_SOURCES = \
	rados-cluster.cpp \
	rados-storage.cpp \
	rados-dictionary.cpp \
	rados-mail-object.cpp 

headers = \
	rados-cluster.h \
	rados-storage.h \
	rados-dictionary.h \
	rados-mail-object.h

pkginc_libdir=$(pkgincludedir)
pkginc_lib_HEADERS = $(headers)
