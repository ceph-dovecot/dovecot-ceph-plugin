aclocaldir = $(datadir)/aclocal
ACLOCAL_AMFLAGS=-I m4

SUBDIRS = \
	. \
	lib \
	src 

EXTRA_DIST = \
	COPYING \
	ChangeLog

nodist_pkginclude_HEADERS = config-local.h dovecot-rados-plugins-config.h

ChangeLog:
	git log --name-status --pretty="format:%ai %aN <%aE> (%h)%n%n%w(80,4,4)%s%n%n%b" . > ChangeLog || rm -f ChangeLog

DISTCLEANFILES = \
	$(top_builddir)/run-test.sh
	