AC_INIT([dovecot-rbox-plugin], [1.0], [p.mauritius@tallence.com])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([dovecot-rbox-plugin-config.h])

AM_INIT_AUTOMAKE([no-define foreign tar-ustar subdir-objects])

AM_MAINTAINER_MODE

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL
	 
DC_DOVECOT
DC_DOVECOT_MODULEDIR
LIBDOVECOT_INCLUDE="$LIBDOVECOT_INCLUDE"
CFLAGS="$DOVECOT_CFLAGS"
LIBS="$DOVECOT_LIBS"

AC_MSG_RESULT([$gcc_debug])
if test "$gcc_debug" = yes; then
    AC_SUBST([CXXFLAGS],"$GCC_DEBUG_CXXFLAGS")
    AC_DEFINE([DEBUG],[],[Debug Mode])
else
    AC_DEFINE([NDEBUG],[],[Release Mode])
fi

# Configure pthreads.
ACX_PTHREAD([have_pthread=yes])

# Define gtest variables 
GTEST_VERSION="1.8.0"
if test "x$have_pthread" = "xyes"; then
  GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=1"
  GTEST_CXXFLAGS="$PTHREAD_CFLAGS -fpermissive -std=c++11"
  GTEST_LDFLAGS="-module -avoid-version"
  GTEST_LIBS="$PTHREAD_LIBS"
else
  GTEST_CPPFLAGS="-DGTEST_HAS_PTHREAD=0"
  GTEST_CXXFLAGS=
  GTEST_LDFLAGS=
  GTEST_LIBS=
fi

AC_SUBST(LIBDOVECOT_INCLUDE)
AC_SUBST([GTEST_VERSION])
AC_SUBST([GTEST_CPPFLAGS])
AC_SUBST([GTEST_CXXFLAGS])
AC_SUBST([GTEST_LDFLAGS])
AC_SUBST([GTEST_LIBS])

AC_CHECK_HEADER(rados/librados.h, [
  have_librados=yes
  AC_DEFINE(HAVE_LIBRADOS,, [Define if you have the RADOS library])
  LIBS="$LIBS -lrados"
], [
    AC_ERROR([Can't build with RADOS support: librados.h not found])
])

AC_CONFIG_FILES([
Makefile
src/Makefile
src/librmb/Makefile
src/dict-rados/Makefile
src/storage-rbox/Makefile
lib/Makefile
lib/gtest/Makefile
src/tests/Makefile

])

AC_OUTPUT
 
AC_MSG_RESULT([
minimal-gtest-autotools $VERSION is now configured
Configure Information:
  C Compiler        : $CC
    DEFS            :   $DEFS
    CPPFLAGS        :   $CPPFLAGS
    CFLAGS          :   $CFLAGS

  C++ Compiler      : $CXX
    DEFS            :   $DEFS
    CPPFLAGS        :   $CPPFLAGS
    CXXFLAGS        :   $CXXFLAGS

  Linker            : $LD
    LDFLAGS         :   $LDFLAGS
    LIBS            :   $LIBS

  Google Test 
    GTEST_CPPFLAGS  : $GTEST_CPPFLAGS
    GTEST_CXXFLAGS  : $GTEST_CXXFLAGS
    GTEST_LDFLAGS   : $GTEST_LDFLAGS
    GTEST_LIBS      : $GTEST_LIBS
])
