# Nonstandard package files for distribution
EXTRA_DIST = \
  ../../src/googletest/googletest/include/gtest/gtest-param-test.h.pump \
  ../../src/googletest/googletest/include/gtest/internal/gtest-param-util-generated.h.pump \
  ../../src/googletest/googletest/include/gtest/internal/gtest-tuple.h.pump \
  ../../src/googletest/googletest/include/gtest/internal/gtest-type-util.h.pump 

# gtest source files that we don't compile directly.  They are
# #included by gtest-all.cc.
GTEST_SRC = \
  ../../src/googletest/googletest/src/gtest-death-test.cc \
  ../../src/googletest/googletest/src/gtest-filepath.cc \
  ../../src/googletest/googletest/src/gtest-internal-inl.h \
  ../../src/googletest/googletest/src/gtest-port.cc \
  ../../src/googletest/googletest/src/gtest-printers.cc \
  ../../src/googletest/googletest/src/gtest-test-part.cc \
  ../../src/googletest/googletest/src/gtest-typed-test.cc \
  ../../src/googletest/googletest/src/gtest.cc \
  ../../src/googletest/googletest/include/gtest/internal/gtest-param-util-generated.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-type-util.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-param-util.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-port.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-linked_ptr.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-internal.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-death-test-internal.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-filepath.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-string.h \
  ../../src/googletest/googletest/include/gtest/internal/gtest-tuple.h \
  ../../src/googletest/googletest/include/gtest/gtest.h \
  ../../src/googletest/googletest/include/gtest/gtest-typed-test.h \
  ../../src/googletest/googletest/include/gtest/gtest-message.h \
  ../../src/googletest/googletest/include/gtest/gtest-test-part.h \
  ../../src/googletest/googletest/include/gtest/gtest_pred_impl.h \
  ../../src/googletest/googletest/include/gtest/gtest-printers.h \
  ../../src/googletest/googletest/include/gtest/gtest-death-test.h \
  ../../src/googletest/googletest/include/gtest/gtest-param-test.h \
  ../../src/googletest/googletest/include/gtest/gtest-spi.h \
  ../../src/googletest/googletest/include/gtest/gtest_prod.h 

EXTRA_DIST += $(GTEST_SRC)

# We define the global AM_CPPFLAGS as everything we compile includes from these
# directories.
AM_CPPFLAGS = -isystem $(srcdir)/include -I$(srcdir) -I../../src/googletest/googletest -I../../src/googletest/googletest/include $(GTEST_CPPFLAGS)
AM_CXXFLAGS = $(GTEST_CXXFLAGS)
AM_LDFLAGS = $(GTEST_LDFLAGS)
LDADD = $(GTEST_LIBS)

# Build rules for libraries.
check_LTLIBRARIES = lib/libgtest.la lib/libgtest_main.la
lib_libgtest_la_SOURCES = ../../src/googletest/googletest/src/gtest-all.cc

lib_libgtest_main_la_SOURCES = ../../src/googletest/googletest/src/gtest_main.cc
lib_libgtest_main_la_LIBADD = lib/libgtest.la

