AC_PREREQ([2.59])

AC_INIT([Dovecot RADOS plugins], [0.0.1], [https://github.com/tallence/dovecot-ceph-plugin/issues/new], ,[https://github.com/tallence/dovecot-ceph-plugin])
AC_CONFIG_AUX_DIR([.])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([no-define foreign tar-ustar subdir-objects])
LT_INIT([shared disable-static pic-only])
AC_ENABLE_SHARED
AC_DISABLE_STATIC

AM_MAINTAINER_MODE
PKG_PROG_PKG_CONFIG

dnl TEST_WITH(name, value, [plugin])
AC_DEFUN([TEST_WITH], [
  want=want_`echo $1|sed s/-/_/g`
  if test $2 = yes || test $2 = no || test $2 = auto; then
    eval $want=$2
  elif test $2 = plugin; then
    if test "$3" = plugin; then
      eval $want=plugin
    else
      AC_ERROR([--with-$1=plugin not supported])
    fi
  elif `echo $2|grep '^/' >/dev/null`; then
    AC_ERROR([--with-$1=path not supported. You may want to use instead:
CPPFLAGS=-I$2/include LDFLAGS=-L$2/lib ./configure --with-$1])
  else
    AC_ERROR([--with-$1: Unknown value: $2])
  fi
])

# Look for RADOS
have_librados=no
AC_CHECK_HEADER([rados/librados.h], [
  have_librados=yes
  AC_DEFINE([HAVE_LIBRADOS], [], [Define if you have the RADOS library])
  LIBS="$LIBS -lrados"
], [AC_ERROR([Can't build with RADOS support: librados.h not found])])


# Evaluate with options
AC_ARG_WITH(dict,
AS_HELP_STRING([--with-dict[=ARG]], [Build with [ARG=yes] or without [ARG=no] RADOS dictionary plugin (yes)]),
  TEST_WITH(dict, $withval),
  want_dict=yes)
AM_CONDITIONAL(BUILD_DICT_RADOS, test "$want_dict" = "yes")

AC_ARG_WITH(storage,
AS_HELP_STRING([--with-storage[=ARG]], [Build with [ARG=yes] or without [ARG=no] RADOS storage plugin (yes)]),
  TEST_WITH(storage, $withval),
  want_storage=yes)
AM_CONDITIONAL(BUILD_STORAGE_RBOX, test "$want_storage" = "yes")

AC_ARG_WITH(tests,
AS_HELP_STRING([--with-tests[=ARG]], [Build with [ARG=yes] or without [ARG=no] RADOS librmb tests (yes)]),
  TEST_WITH(tests, $withval),
  want_tests=yes)
AM_CONDITIONAL(BUILD_LIBRMB_TESTS, test "$want_tests" = "yes")

AC_PROG_CC

AC_PROG_CXX
AX_CXX_COMPILE_STDCXX([11])

AC_PROG_CPP
AC_PROG_LIBTOOL

AC_ISC_POSIX
AC_C_INLINE

# Couple Dovecot config
DC_DOVECOT
DC_DOVECOT_MODULEDIR

LIBS+=" $DOVECOT_LIBS"
LIBDOVECOT_INCLUDE="$LIBDOVECOT_INCLUDE $LIBDOVECOT_STORAGE_INCLUDE"

BINARY_CFLAGS="$DOVECOT_BINARY_CFLAGS"
BINARY_LDFLAGS="$DOVECOT_BINARY_LDFLAGS"

CFLAGS+=" $DOVECOT_CFLAGS $EXTRA_CFLAGS -I\$(top_srcdir)"
CXXFLAGS+=" -Wall -Wextra -fpermissive -std=c++11 $lt_cv_prog_compiler_pic_CXX"

AC_SUBST(BINARY_CFLAGS)
AC_SUBST(BINARY_LDFLAGS)
AC_SUBST(LIBDOVECOT_INCLUDE)


AC_DEFINE([PACKAGE_VERSION_MAJOR], regexp(AC_PACKAGE_VERSION, [^\([0-9]+\)\.\([0-9]+\)], [\1]), [Dovecot RADOS plugin major version])
AC_DEFINE([PACKAGE_VERSION_MINOR], regexp(AC_PACKAGE_VERSION, [^\([0-9]+\)\.\([0-9]+\)], [\2]), [Dovecot RADOS plugin minor version])

AC_MSG_CHECKING([whether Dovecot supports dict_vfuncs.switch_ioloop])
grep switch_ioloop $dovecot_pkgincludedir/dict-private.h > /dev/null 2>&1
AS_IF([test "$?" = 0], [AC_MSG_RESULT(yes) AC_DEFINE([HAVE_DICT_SWITCH_IOLOOP],,[dict_vfuncs.switch_ioloop supported])],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([whether Dovecot supports dict_vfuncs.set_timestamp])
grep set_timestamp $dovecot_pkgincludedir/dict-private.h > /dev/null 2>&1
AS_IF([test "$?" = 0], [AC_MSG_RESULT(yes) AC_DEFINE([HAVE_DICT_SET_TIMESTAMP],,[dict_vfuncs.set_timestamp supported])],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([whether Dovecot supports index-pop3-uidl.h])
AS_IF([test -f $dovecot_pkgincludedir/index-pop3-uidl.h], [AC_MSG_RESULT(yes) AC_DEFINE([HAVE_INDEX_POP3_UIDL_H],,[index-pop3-uidl.h found])],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([whether Dovecot supports mail_vfuncs.index_mail_set_cache_corrupted_reason])
grep index_mail_set_cache_corrupted_reason $dovecot_pkgincludedir/index-mail.h > /dev/null 2>&1
AS_IF([test "$?" = 0], [AC_MSG_RESULT(yes) AC_DEFINE([HAVE_INDEX_MAIL_SET_CACHE_CORRUPTED_REASON],,[mail_vfuncs.index_mail_set_cache_corrupted_reason supported])],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([whether Dovecot supports mail_save_context.copy_src_mail])
grep copy_src_mail $dovecot_pkgincludedir/mail-storage-private.h > /dev/null 2>&1
AS_IF([test "$?" = 0], [AC_MSG_RESULT(yes) AC_DEFINE([HAVE_MAIL_SAVE_CONTEXT_COPY_SRC_MAIL],,[mail_save_context.copy_src_mail supported])],[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([whether Dovecot supports MAIL_INDEX_HDR_FLAG_FSCKD])
grep MAIL_INDEX_HDR_FLAG_FSCKD $dovecot_pkgincludedir/mail-index.h > /dev/null 2>&1
AS_IF([test "$?" = 0], [AC_MSG_RESULT(yes) AC_DEFINE([HAVE_MAIL_INDEX_HDR_FLAG_FSCKD],,[MAIL_INDEX_HDR_FLAG_FSCKD supported])],[AC_MSG_RESULT(no)])

AC_CONFIG_HEADERS([config-local.h])
AX_PREFIX_CONFIG_H([], [], [config-local.h])

# Active gtest
AC_MSG_NOTICE([$gcc_debug])
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

AC_SUBST([GTEST_VERSION])
AC_SUBST([GTEST_CPPFLAGS])
AC_SUBST([GTEST_CXXFLAGS])
AC_SUBST([GTEST_LDFLAGS])
AC_SUBST([GTEST_LIBS])

# finish

AC_CONFIG_FILES([
Makefile
src/Makefile
src/librmb/Makefile
src/dict-rados/Makefile
src/storage-rbox/Makefile
src/librmb/tools/Makefile
src/librmb/tools/rmb/Makefile
lib/Makefile
lib/gtest/Makefile
src/tests/Makefile
])

AC_OUTPUT

echo
AC_MSG_NOTICE([Install prefix .... : $prefix])
AC_MSG_NOTICE([Dovecot directory . : $dovecotdir])
AC_MSG_NOTICE([With dictionary ... : $want_dict])
AC_MSG_NOTICE([With storage ...... : $want_storage])
AC_MSG_NOTICE([With tests ........ : $want_tests])

if test "$want_tests" = yes; then
AC_MSG_NOTICE([

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
fi
