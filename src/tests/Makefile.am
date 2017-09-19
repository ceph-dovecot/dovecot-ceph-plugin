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

TESTS += it_test_librmb
check_PROGRAMS += it_test_librmb
it_test_librmb_SOURCES = librmb/it_test_librmb.cpp
it_test_librmb_LDADD = $(rmb_shlibs) $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o $(top_srcdir)/src/librmb/tools/rmb/mailbox_tools.o  libgtest.la libgmock.la
    
TESTS += test_rmb
check_PROGRAMS += test_rmb
test_rmb_SOURCES = rmb/test_rmb.cpp
test_rmb_LDADD = $(rmb_shlibs) $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o  $(top_srcdir)/src/librmb/tools/rmb/mailbox_tools.o libgtest.la libgmock.la
    
dict_shlibs = \
	$(top_srcdir)/src/dict-rados/libdict_rados_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)

TESTS += it_test_dict_rados
check_PROGRAMS += it_test_dict_rados
it_test_dict_rados_SOURCES = dict-rados/it_test_dict_rados.cpp dict-rados/TestCase.cpp 
it_test_dict_rados_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_dict_rados_LDADD = $(dict_shlibs) libgtest.la libgmock.la

storage_shlibs = \
	$(top_srcdir)/src/storage-rbox/libstorage_rbox_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)

TESTS += it_test_storage_rbox
check_PROGRAMS += it_test_storage_rbox
it_test_storage_rbox_SOURCES = storage-rbox/it_test_storage_rbox.cpp storage-rbox/TestCase.cpp 
it_test_storage_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_storage_rbox_LDADD = $(storage_shlibs) libgtest.la libgmock.la


TESTS += test_storage_mock_rbox
check_PROGRAMS += test_storage_mock_rbox
test_storage_mock_rbox_SOURCES = storage-mock-rbox/test_storage_mock_rbox.cpp storage-mock-rbox/TestCase.cpp 
test_storage_mock_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
test_storage_mock_rbox_LDADD = $(storage_shlibs) libgtest.la libgmock.la

    