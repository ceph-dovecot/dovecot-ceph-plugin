SUBDIRS = src

ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	ChangeLog

ChangeLog:
	git log --name-status --pretty="format:%ai %aN <%aE> (%h)%n%n%w(80,4,4)%s%n%n%b" . > ChangeLog || rm -f ChangeLog
