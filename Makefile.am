aclocaldir = $(datadir)/aclocal
ACLOCAL_AMFLAGS=-I m4

SUBDIRS = \
	. \
	src 

EXTRA_DIST = \
	COPYINGq \
	ChangeLog

EXTRA_DIST = \
	COPYING.LGPL \
	COPYING.MIT \
	ChangeLog \
	update-version.sh \
	run-test-valgrind.supp \
	$(conf_DATA)

nodist_pkginclude_HEADERS = config-local.h dovecot-rados-plugins-config.h

ChangeLog:
	git log --name-status --pretty="format:%ai %aN <%aE> (%h)%n%n%w(80,4,4)%s%n%n%b" . > ChangeLog || rm -f ChangeLog

DISTCLEANFILES = \
	$(top_builddir)/run-test.sh
	