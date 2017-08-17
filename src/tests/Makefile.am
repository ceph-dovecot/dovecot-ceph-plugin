
AM_CPPFLAGS = -isystem $(top_srcdir)/lib/gtest/include -I$(top_srcdir)/src  -I$(top_srcdir)/src/librmb $(GTEST_CPPFLAGS)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)
AM_LDFLAGS = $(GTEST_LDFLAGS) $(GTEST_LIBS) \
	     $(top_srcdir)/lib/gtest/lib/libgtest_main.la \
	     $(top_srcdir)/lib/gtest/lib/libgtest.la


# TESTS -- Programs run automatically by "make check"
# check_PROGRAMS -- Programs built by "make check" but not necessarily run
TESTS = 
check_PROGRAMS = 

shlibs = \
	$(top_srcdir)/src/librmb/librmb.la


TESTS += librmb_test
check_PROGRAMS += librmb_test
librmb_test_SOURCES = librmb_test.cpp
librmb_test_LDADD = $(shlibs)  