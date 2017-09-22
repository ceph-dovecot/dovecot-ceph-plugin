#
# spec file for package dovecot22-rados-plugins
#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

%define librados_version  10.2.5

Name:		dovecot-ceph-plugin
Summary:	Dovecot Ceph RADOS plugins
Version:	0.0.1
Release:	0%{?dist}
URL:		https://github.com/ceph-dovecot/dovecot-ceph-plugin
Group:		Productivity/Networking/Email/Servers
License:	LGPL-2.1
Source:		%{name}-%{version}.tar.xz

Provides:	dovecot-rados-plugins = %{version}-%{release}
Requires:	librmb0 >= %{version}-%{release}
Requires:	librados2 >= %{librados_version}
Conflicts:	otherproviders(dovecot-rados-plugins)

BuildRoot:	%{_tmppath}/%{name}-%{version}-build
BuildRequires:	dovecot22-devel
BuildRequires:	librados-devel >= %{librados_version}
BuildRequires:	gcc-c++
BuildRequires:	libtool

%description
Dovecot is an IMAP and POP3 server for Linux and UNIX-like systems,
written primarily with security in mind. Although it is written in C,
it uses several coding techniques to avoid most of the common pitfalls.

Dovecot can work with standard mbox and maildir formats and is fully
compatible with UW-IMAP and Courier IMAP servers as well as mail
clients accessing the mailboxes directly.

This package holds the files needed for RADOS support.

%package -n librmb0
Summary:	RADOS mailbox library
Group:		System/Libraries
%description -n librmb0
Library with generic abstraction to store emails in RADOS

%package -n librmb-devel
Summary:	RADOS mailbox headers
Requires: 	librmb0 >= %{version}-%{release}
Group:		Development/Libraries/C and C++
%description -n librmb-devel
This package contains libraries and headers needed to develop programs
that use rados mailbox library.

%package -n rmb-tools
Summary:	RADOS mailbox tools
Requires: 	librmb0 >= %{version}-%{release}
Group:		Productivity/Networking/Email/Servers
%description -n rmb-tools
This package contains useful tools to manage RADOS mailbox setups.

%prep
%setup -q

%build
export CFLAGS="%{optflags}"
export CFLAGS="$CFLAGS -fpic -DPIC"
export LIBS="-pie"

./autogen.sh
%configure \
	--prefix=%{_prefix} \
	--with-dovecot=%{_libdir}dovecot
%{__make}

%install
%makeinstall

# clean up unused files
find %{buildroot}%{_libdir}/ -type f -name \*.la -delete
find %{buildroot}%{_libdir}/dovecot/ -type f -name \*.a  -delete

%clean
%{__rm} -rf %{buildroot}

%post
/sbin/ldconfig
%postun
/sbin/ldconfig

%files
%defattr(-,root,root)
%dir %{_libdir}/dovecot
%{_libdir}/dovecot/lib*.so*

%files -n librmb0
%{_libdir}/librmb.so.*

%post -n librmb0 -p /sbin/ldconfig

%postun -n librmb0 -p /sbin/ldconfig

%files -n librmb-devel
# TODO: add header files
%{_libdir}/librmb.so

%files -n rmb-tools
%defattr(0755,root,root)
%{_bindir}/rmb

%changelog
