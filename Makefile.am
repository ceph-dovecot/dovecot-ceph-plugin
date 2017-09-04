#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

aclocaldir = $(datadir)/aclocal
ACLOCAL_AMFLAGS=-I m4

SUBDIRS = \
	. \
	src 

EXTRA_DIST = \
	COPYING \
	ChangeLog

nodist_pkginclude_HEADERS = config-local.h dovecot-ceph-plugins-config.h

ChangeLog:
	git log --name-status --pretty="format:%ai %aN <%aE> (%h)%n%n%w(80,4,4)%s%n%n%b" . > ChangeLog || rm -f ChangeLog

DISTCLEANFILES = \
	$(top_builddir)/run-test.sh
	