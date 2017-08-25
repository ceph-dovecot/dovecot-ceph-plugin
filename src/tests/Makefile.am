
AM_CPPFLAGS = -isystem $(top_srcdir)/src/googletest/googletest/include -I$(top_srcdir)/src  -I$(top_srcdir)/src/librmb -I$(top_srcdir)/src/dict-rados $(GTEST_CPPFLAGS)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)

TESTS = 
check_PROGRAMS = 

check_LTLIBRARIES = libgtest.la
libgtest_la_SOURCES = ../googletest/googletest/src/gtest-all.cc
libgtest_la_CPPFLAGS = -I$(top_srcdir)/src/googletest/googletest/include -I$(top_srcdir)/src/googletest/googletest
libgtest_la_LDFLAGS = -pthread

rmb_shlibs = \
	$(top_srcdir)/src/librmb/librmb.la 

TESTS += librmb_test
check_PROGRAMS += librmb_test
librmb_test_SOURCES = librmb/librmb_test.cpp
librmb_test_LDADD = $(rmb_shlibs) $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o $(top_srcdir)/src/librmb/tools/rmb/mailbox_tools.o  libgtest.la
    
TESTS += rmb_test
check_PROGRAMS += rmb_test
rmb_test_SOURCES = rmb/rmb_test.cpp
rmb_test_LDADD = $(rmb_shlibs)  $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o  $(top_srcdir)/src/librmb/tools/rmb/mailbox_tools.o libgtest.la
    
dict_shlibs = \
	$(top_srcdir)/src/dict-rados/libdict-rados.la $(rmb_shlibs)

#TESTS += dict_test
#check_PROGRAMS += dict_test
#dict_test_SOURCES = dict-rados/dict_test.cpp
#dict_test_LDADD = $(dict_shlibs)  $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o  $(top_srcdir)/src/librmb/tools/rmb/mailbox_tools.o libgtest.la
    