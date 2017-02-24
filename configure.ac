AC_INIT([librados-plugin], [1.0], [p.mauritius@tallence.com])
AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([librados-plugin-config.h])

AM_INIT_AUTOMAKE([no-define foreign tar-ustar])

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
AC_SUBST(LIBDOVECOT_INCLUDE)

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
])

AC_OUTPUT
 