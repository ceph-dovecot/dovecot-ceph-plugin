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
	rados-cluster.h \
	rados-dictionary.h \
	rados-storage.h \
	encoding.h \
	rados-cluster-impl.h \
	rados-storage-impl.h \
	rados-dictionary-impl.h \
	rados-mail-object.h \
	rados-util.h \
	rados-metadata.h \
	rados-types.h \
	rados-dovecot-config.h \
	rados-namespace-manager.h \
	rados-dovecot-ceph-cfg.h \
	rados-dovecot-ceph-cfg-impl.h \
	rados-ceph-config.h \
	rados-ceph-json-config.h

librmb_la_SOURCES = \
    $(headers) \
	rados-cluster-impl.cpp \
	rados-storage-impl.cpp \
	rados-dictionary-impl.cpp \
	rados-mail-object.cpp \
	rados-util.cpp \
	rados-dovecot-config.cpp \
	rados-namespace-manager.cpp\
	rados-ceph-config.cpp \
	rados-dovecot-ceph-cfg-impl.cpp \
	rados-ceph-json-config.cpp
	
AM_LDFLAGS = -ljansson

pkginc_libdir=$(pkgincludedir)
pkginc_lib_HEADERS = $(headers)

