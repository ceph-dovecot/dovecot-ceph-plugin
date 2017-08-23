
AM_CPPFLAGS = -isystem $(top_srcdir)/src/googletest/googletest/include -I$(top_srcdir)/src  -I$(top_srcdir)/src/librmb $(GTEST_CPPFLAGS)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)
AM_LDFLAGS = $(GTEST_LDFLAGS) $(GTEST_LIBS) \
	     $(top_srcdir)/lib/gtest/lib/libgtest_main.la \
	     $(top_srcdir)/lib/gtest/lib/libgtest.la

# this will also work when (googletest/googletest/make/Makefile) was called
#AM_LDFLAGS = $(GTEST_LDFLAGS) $(GTEST_LIBS) \
#	     $(top_srcdir)/src/googletest/googletest/make/gtest_main.o \
#	     $(top_srcdir)/src/googletest/googletest/make/gtest-all.o


# TESTS -- Programs run automatically by "make check"
# check_PROGRAMS -- Programs built by "make check" but not necessarily run
TESTS = 
check_PROGRAMS = 

shlibs = \
	$(top_srcdir)/src/librmb/librmb.la 


TESTS += librmb_test
check_PROGRAMS += librmb_test
librmb_test_SOURCES = librmb/librmb_test.cpp
librmb_test_LDADD = $(shlibs) $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o 
    
TESTS += rmb_test
check_PROGRAMS += rmb_test
rmb_test_SOURCES = rmb/rmb_test.cpp
rmb_test_LDADD = $(shlibs)  $(top_srcdir)/src/librmb/tools/rmb/ls_cmd_parser.o 
    