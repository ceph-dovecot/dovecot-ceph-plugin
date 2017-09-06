#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

AM_CPPFLAGS = -isystem $(top_srcdir)/src/googletest/googletest/include  -I$(top_srcdir)/src/googletest/googlemock/include -I$(top_srcdir)/src  -Imocks -I$(top_srcdir)/src/librmb -I$(top_srcdir)/src/dict-rados -I$(top_srcdir)/src/storage-rbox $(GTEST_CPPFLAGS)
AM_CXXFLAGS = -g -O0 $(GTEST_CXXFLAGS)

TESTS = 
check_PROGRAMS = 

check_LTLIBRARIES = libgtest.la
libgtest_la_SOURCES = ../googletest/googletest/src/gtest-all.cc
libgtest_la_CPPFLAGS = -I$(top_srcdir)/src/googletest/googletest/include -I$(top_srcdir)/src/googletest/googletest
libgtest_la_LDFLAGS = -pthread


check_LTLIBRARIES += libgmock.la
libgmock_la_SOURCES = ../googletest/googlemock/src/gmock-all.cc
libgmock_la_CPPFLAGS = -I$(top_srcdir)/src/googletest/googlemock/include -I$(top_srcdir)/src/googletest/googlemock -I$(top_srcdir)/src/googletest/googletest/include -I$(top_srcdir)/src/googletest/googletest
libgmock_la_LDFLAGS = -pthread


rmb_shlibs = \
	$(top_srcdir)/src/librmb/librmb.la 

TESTS += librmb_test
check_PROGRAMS += librmb_test
librmb_test_SOURCES = librmb/librmb_test.cpp
librmb_test_LDADD = $(rmb_shlibs) $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o $(top_srcdir)/src/librmb/tools/rmb/mailbox_tools.o  libgtest.la libgmock.la
    
TESTS += rmb_test
check_PROGRAMS += rmb_test
rmb_test_SOURCES = rmb/rmb_test.cpp
rmb_test_LDADD = $(rmb_shlibs) $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o  $(top_srcdir)/src/librmb/tools/rmb/mailbox_tools.o libgtest.la libgmock.la
    
dict_shlibs = \
	$(top_srcdir)/src/dict-rados/libdict_rados_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)

TESTS += dict_rados_test
check_PROGRAMS += dict_rados_test
dict_rados_test_SOURCES = dict-rados/dict_rados_test.cpp dict-rados/TestCase.cpp 
dict_rados_test_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
dict_rados_test_LDADD = $(dict_shlibs) libgtest.la libgmock.la

storage_shlibs = \
	$(top_srcdir)/src/storage-rbox/libstorage_rbox_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)

TESTS += storage_rbox_test
check_PROGRAMS += storage_rbox_test
storage_rbox_test_SOURCES = storage-rbox/storage_rbox_test.cpp storage-rbox/TestCase.cpp 
storage_rbox_test_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
storage_rbox_test_LDADD = $(storage_shlibs) libgtest.la libgmock.la
    