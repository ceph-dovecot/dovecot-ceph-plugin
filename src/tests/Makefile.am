#
# Copyright (c) 2017-2018 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

AM_CPPFLAGS = \
    -I$(top_srcdir) -I$(top_srcdir)/src/tests/mocks -I$(top_srcdir)/src/librmb -I$(top_srcdir)/src/dict-rados -I$(top_srcdir)/src/storage-rbox \
    -I$(srcdir)/googletest/googletest/include \
    -I$(srcdir)/googletest/googlemock/include \
    $(LIBDOVECOT_INCLUDE) \
    $(GTEST_CPPFLAGS) \
    $(CODE_COVERAGE_CPPFLAGS) 

AM_CXXFLAGS = \
    $(GTEST_CXXFLAGS) \
    $(CODE_COVERAGE_CXXFLAGS)
    
AM_LDFLAGS = $(GTEST_LDFLAGS)

EXTRA_DIST = googletest mocks dict_rados.supp open_connection.supp

rmb_shlibs = \
	$(top_builddir)/src/librmb/librmb.la $(CODE_COVERAGE_LIBS)
dict_shlibs = \
	$(top_builddir)/src/dict-rados/libdict_rados_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)
storage_shlibs = \
	$(top_builddir)/src/storage-rbox/libstorage_rbox_plugin.la $(LIBDOVECOT_STORAGE) $(LIBDOVECOT) $(rmb_shlibs)

doveadm_rmb_shlibs = \
	$(top_builddir)/src/storage-rbox/lib10_doveadm_rbox_plugin.la $(LIBDOVECOT_DEPS) 


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

#noinst_LIBRARIES = libtest_utils.a
#libtest_utils_a_SOURCES = test-utils/it_utils.cpp  test-utils/it_utils.h
#libtest_utils_a_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
#libtest_utils_a_LIBADD = $(gtest_shlibs) $(storage_shlibs)

TESTS = test_rmb
test_rmb_SOURCES = rmb/test_rmb.cpp mocks/mock_test.h
test_rmb_LDADD = $(rmb_shlibs) $(top_builddir)/src/librmb/tools/rmb/ls_cmd_parser.o $(top_builddir)/src/librmb/tools/rmb/rmb-commands.o   $(top_builddir)/src/librmb/tools/rmb/mailbox_tools.o $(gtest_shlibs)
    
TESTS += test_storage_mock_rbox
test_storage_mock_rbox_SOURCES = storage-mock-rbox/test_storage_mock_rbox.cpp storage-mock-rbox/TestCase.cpp storage-mock-rbox/TestCase.h mocks/mock_test.h test-utils/it_utils.cpp test-utils/it_utils.h 
test_storage_mock_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
test_storage_mock_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += test_storage_mock_rbox_bugs
test_storage_mock_rbox_bugs_SOURCES = storage-mock-rbox/test_storage_mock_rbox_bugs.cpp storage-mock-rbox/TestCase.cpp storage-mock-rbox/TestCase.h mocks/mock_test.h test-utils/it_utils.cpp test-utils/it_utils.h 
test_storage_mock_rbox_bugs_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
test_storage_mock_rbox_bugs_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += test_repair_rbox
test_repair_rbox_SOURCES = storage-mock-rbox/test_repair_rbox.cpp storage-mock-rbox/TestCase.cpp storage-mock-rbox/TestCase.h mocks/mock_test.h test-utils/it_utils.cpp test-utils/it_utils.h 
test_repair_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
test_repair_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 


TESTS += test_librmb_utils
test_librmb_utils_SOURCES = librmb/test_librmb_utils.cpp
test_librmb_utils_LDADD = $(rmb_shlibs) $(top_builddir)/src/librmb/tools/rmb/ls_cmd_parser.o  $(top_builddir)/src/librmb/tools/rmb/mailbox_tools.o $(gtest_shlibs)
    
if BUILD_INTEGRATION_TESTS


TESTS += it_test_librmb
it_test_librmb_SOURCES = librmb/it_test_librmb.cpp 
it_test_librmb_LDADD = $(rmb_shlibs) $(top_builddir)/src/librmb/tools/rmb/ls_cmd_parser.o  $(top_builddir)/src/librmb/tools/rmb/rmb-commands.o $(top_builddir)/src/librmb/tools/rmb/mailbox_tools.o $(gtest_shlibs)

TESTS += it_test_dict_rados
it_test_dict_rados_SOURCES = dict-rados/it_test_dict_rados.cpp dict-rados/TestCase.cpp dict-rados/TestCase.h  
it_test_dict_rados_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_dict_rados_LDADD = $(dict_shlibs) $(gtest_shlibs) 

TESTS += it_test_storage_rbox
it_test_storage_rbox_SOURCES = storage-rbox/it_test_storage_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h  
it_test_storage_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_storage_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_storage_rbox_2
it_test_storage_rbox_2_SOURCES = storage-rbox/it_test_storage_rbox.cpp storage-rbox2/TestCase.cpp storage-rbox2/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h  
it_test_storage_rbox_2_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_storage_rbox_2_LDADD = $(storage_shlibs) $(gtest_shlibs) 


TESTS += it_test_copy_rbox
it_test_copy_rbox_SOURCES = storage-rbox/it_test_copy_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h 
it_test_copy_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_copy_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_check_metadata
it_test_check_metadata_SOURCES = storage-rbox/it_test_check_metadata.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h 
it_test_check_metadata_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_check_metadata_LDADD = $(storage_shlibs) $(gtest_shlibs)
 
TESTS += it_test_copy_rbox_alt
it_test_copy_rbox_alt_SOURCES = storage-rbox/it_test_copy_rbox_alt.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h 
it_test_copy_rbox_alt_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_copy_rbox_alt_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_copy_rbox_fail
it_test_copy_rbox_fail_SOURCES = storage-rbox/it_test_copy_rbox_fail.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h 
it_test_copy_rbox_fail_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_copy_rbox_fail_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_move_rbox
it_test_move_rbox_SOURCES = storage-rbox/it_test_move_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_move_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_move_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs)

TESTS += it_test_move_rbox_alt
it_test_move_rbox_alt_SOURCES = storage-rbox/it_test_move_rbox_alt.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_move_rbox_alt_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_move_rbox_alt_LDADD = $(storage_shlibs) $(gtest_shlibs)

TESTS += it_test_lda_rbox
it_test_lda_rbox_SOURCES = storage-rbox/it_test_lda_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h
it_test_lda_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_lda_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_doveadm_backup_rbox
it_test_doveadm_backup_rbox_SOURCES = storage-rbox/it_test_doveadm_backup_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp  test-utils/it_utils.h
it_test_doveadm_backup_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_doveadm_backup_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_read_mail_rbox
it_test_read_mail_rbox_SOURCES = storage-rbox/it_test_read_mail_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_read_mail_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_read_mail_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_read_mail_rbox_alt
it_test_read_mail_rbox_alt_SOURCES = storage-rbox/it_test_read_mail_rbox_alt.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_read_mail_rbox_alt_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_read_mail_rbox_alt_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_read_mail_failed_rbox
it_test_read_mail_failed_rbox_SOURCES = storage-rbox/it_test_read_mail_failed_rbox.cpp storage-rbox/TestCase.cpp storage-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_read_mail_failed_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_read_mail_failed_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_sync_rbox
it_test_sync_rbox_SOURCES = sync-rbox/it_test_sync_rbox.cpp sync-rbox/TestCase.cpp sync-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_sync_rbox_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE)  
it_test_sync_rbox_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_sync_rbox_2
it_test_sync_rbox_2_SOURCES = sync-rbox/it_test_sync_rbox_2.cpp sync-rbox/TestCase.cpp sync-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_sync_rbox_2_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_sync_rbox_2_LDADD = $(storage_shlibs) $(gtest_shlibs) 

TESTS += it_test_sync_rbox_alt
it_test_sync_rbox_alt_SOURCES = sync-rbox/it_test_sync_rbox_alt.cpp sync-rbox/TestCase.cpp sync-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_sync_rbox_alt_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_sync_rbox_alt_LDADD = $(storage_shlibs) $(gtest_shlibs) 


TESTS += it_test_sync_rbox_duplicate_uid
it_test_sync_rbox_duplicate_uid_SOURCES = sync-rbox/it_test_sync_rbox_duplicate_uid.cpp sync-rbox/TestCase.cpp sync-rbox/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
it_test_sync_rbox_duplicate_uid_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE) 
it_test_sync_rbox_duplicate_uid_LDADD = $(storage_shlibs) $(gtest_shlibs) 


## compile issue with centos and dovecot 2.3.15
#TESTS += it_test_doveadm_rmb
#it_test_doveadm_rmb_SOURCES = doveadm-rmb/it_test_doveadm_rmb.cpp doveadm-rmb/TestCase.cpp doveadm-rmb/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
#it_test_doveadm_rmb_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE)  
#it_test_doveadm_rmb_LDADD = $(storage_shlibs) $(gtest_shlibs) $(doveadm_rmb_shlibs) 

#TESTS += it_test_backup
#it_test_backup_SOURCES = backup/it_test_backup_rbox.cpp backup/TestCase.cpp backup/TestCase.h test-utils/it_utils.cpp test-utils/it_utils.h
#it_test_backup_CPPFLAGS = $(AM_CPPFLAGS) $(LIBDOVECOT_INCLUDE)  
#it_test_backup_LDADD = $(storage_shlibs) $(gtest_shlibs) $(doveadm_rmb_shlibs) 

endif

check_PROGRAMS = $(TESTS)
noinst_PROGRAMS = $(TESTS)

@CODE_COVERAGE_RULES@

@VALGRIND_CHECK_RULES@
VALGRIND_SUPPRESSIONS_FILES = dict_rados.supp open_connection.supp dovecot.supp
