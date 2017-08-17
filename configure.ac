AC_INIT([dovecot-rbox-plugin], [1.0.0], [p.mauritius@tallence.com])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([dovecot-rbox-plugin-config.h])

AM_INIT_AUTOMAKE([no-define foreign tar-ustar])

AM_MAINTAINER_MODE

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

AC_PROG_CC
AC_PROG_CXX
AC_PROG_CPP
AC_PROG_LIBTOOL
	 
DC_DOVECOT
DC_DOVECOT_MODULEDIR
LIBDOVECOT_INCLUDE="$LIBDOVECOT_INCLUDE"
CFLAGS="$DOVECOT_CFLAGS"
LIBS="$DOVECOT_LIBS"
AC_SUBST(LIBDOVECOT_INCLUDE)

have_librados=no
AC_CHECK_HEADER(rados/librados.h, [
  have_librados=yes
  AC_DEFINE(HAVE_LIBRADOS,, [Define if you have the RADOS library])
  LIBS="$LIBS -lrados"
], [
    AC_ERROR([Can't build with RADOS support: librados.h not found])
])

AC_ARG_WITH(dict,
AS_HELP_STRING([--with-dict], [Build with RADOS dictionary plugin]),
  TEST_WITH(dict, $withval),
  want_dict=yes)
AM_CONDITIONAL(BUILD_DICT_RADOS, test "$want_dict" = "yes")

AC_ARG_WITH(storage,
AS_HELP_STRING([--with-storage], [Build with RADOS storage plugin]),
  TEST_WITH(storage, $withval),
  want_storage=yes)
AM_CONDITIONAL(BUILD_STORAGE_RBOX, test "$want_storage" = "yes")

AC_CONFIG_FILES([
Makefile
src/Makefile
src/librmb/Makefile
src/dict-rados/Makefile
src/storage-rbox/Makefile
])

AC_OUTPUT
 