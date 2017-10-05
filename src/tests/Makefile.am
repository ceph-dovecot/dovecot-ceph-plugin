#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

AM_CPPFLAGS = -I$(top_srcdir) -I$(top_srcdir)/src/tests/mocks -I$(top_srcdir)/src/librmb -I$(top_srcdir)/src/dict-rados -I$(top_srcdir)/src/storage-rbox \
 -I$(srcdir)/googletest/googletest/include \
 -I$(srcdir)/googletest/googlemock/include 

AM_CXXFLAGS = $(GTEST_CXXFLAGS)
AM_LDFLAGS = $(GTEST_LDFLAGS)

EXTRA_DIST = googletest mocks

rmb_shlibs = \
	$(top_builddir)/src/librmb/librmb.la
dict_shlibs = \
	$(top_builddir)/src/dict-rados/libdict_rados_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)
storage_shlibs = \
	$(top_builddir)/src/storage-rbox/libstorage_rbox_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)

check_LTLIBRARIES = libgmockgtest.la

libgmockgtest_la_CPPFLAGS = \
 -I$(srcdir)/googletest/googletest \
 -I$(srcdir)/googletest/googlemock \
 -I$(srcdir)/googletest/googletest/include \
 -I$(srcdir)/googletest/googlemock/include \
 -lglog

libgmockgtest_la_SOURCES = \
	googletest/googletest/src/gtest-all.cc \
	googletest/googlemock/src/gmock-all.cc

gtest_shlibs = libgmockgtest.la

TESTS = test_rmb
test_rmb_SOURCES = rmb/test_rmb.cpp mocks/mock_test.h
test_rmb_LDADD = $(rmb_shlibs) $(top_builddir)/src/librmb/tools/rmb/ls_cmd_parser.o  $(top_builddir)/src/librmb/tools/rmb/mailbox_tools.o $(gtest_shlibs)
    
TESTS += test_storage_mock_rbox
test_storage_mock_rbox_SOURCES = storage-mock-rbox/test_storage_mock_rbox.cpp storage-mock-rbox/TestCase.cpp storage-mock-rbox/TestCase.h mocks/mock_test.h
test_storage_mock_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
test_storage_mock_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs)

if BUILD_INTEGRATION_TESTS

TESTS += it_test_librmb
it_test_librmb_SOURCES = librmb/it_test_librmb.cpp
it_test_librmb_LDADD = $(rmb_shlibs) $(top_builddir)/src/librmb/tools/rmb/ls_cmd_parser.o $(top_builddir)/src/librmb/tools/rmb/mailbox_tools.o $(gtest_shlibs)
    
TESTS += it_test_dict_rados
it_test_dict_rados_SOURCES = dict-rados/it_test_dict_rados.cpp dict-rados/TestCase.cpp dict-rados/TestCase.h 
it_test_dict_rados_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_dict_rados_LDADD = $(dict_shlibs) $(gtest_shlibs)

TESTS += it_test_storage_rbox
it_test_storage_rbox_SOURCES = storage-rbox/it_test_storage_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h
it_test_storage_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_storage_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs)

TESTS += it_test_copy_rbox
it_test_copy_rbox_SOURCES = storage-rbox/it_test_copy_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h
it_test_copy_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_copy_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs)

TESTS += it_test_move_rbox
it_test_move_rbox_SOURCES = storage-rbox/it_test_move_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h
it_test_move_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_move_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs)

TESTS += it_test_read_mail_rbox
it_test_read_mail_rbox_SOURCES = storage-rbox/it_test_read_mail_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h
it_test_read_mail_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_read_mail_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs)

TESTS += it_test_sync_rbox
it_test_sync_rbox_SOURCES = sync-rbox/it_test_sync_rbox.cpp sync-rbox/TestCase.cpp sync-rbox/TestCase.h
it_test_sync_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_sync_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs)

TESTS += it_test_sync_rbox_2
it_test_sync_rbox_2_SOURCES = sync-rbox/it_test_sync_rbox_2.cpp sync-rbox/TestCase.cpp sync-rbox/TestCase.h
it_test_sync_rbox_2_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_sync_rbox_2_LDADD = $(storage_shlibs) $(gtest_shlibs)

endif

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

