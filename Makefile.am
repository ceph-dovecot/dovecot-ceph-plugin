aclocaldir = $(datadir)/aclocal
ACLOCAL_AMFLAGS=-I m4

SUBDIRS = \
	. \
	src 

EXTRA_DIST = \
	ChangeLog

nodist_pkginclude_HEADERS = config.h dovecot-rados-plugins-config.h

ChangeLog:
	git log --name-status --pretty="format:%ai %aN <%aE> (%h)%n%n%w(80,4,4)%s%n%n%b" . > ChangeLog || rm -f ChangeLog

DISTCLEANFILES = \
	$(top_builddir)/run-test.sh
	