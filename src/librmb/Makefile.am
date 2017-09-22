#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

SUBDIRS = . tools
     
lib_LTLIBRARIES = \
	librmb.la

headers = \
	interfaces/rados-cluster-interface.h \
	interfaces/rados-dictionary-interface.h \
	interfaces/rados-storage-interface.h \
    encoding.h \
	rados-cluster.h \
	rados-storage.h \
	rados-dictionary.h \
	rados-mail-object.h 

librmb_la_SOURCES = \
    $(headers) \
	rados-cluster.cpp \
	rados-storage.cpp \
	rados-dictionary.cpp \
	rados-mail-object.cpp 


pkginc_libdir=$(pkgincludedir)
# 
# skip header installation because all parts are in one projekt right now
# pkginc_lib_HEADERS = $(headers)


