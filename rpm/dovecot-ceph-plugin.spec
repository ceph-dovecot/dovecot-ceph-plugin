#
# spec file for package dovecot-ceph-plugin
#
# Copyright (c) 2017-2018 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

%{!?librados_version: %define librados_version 10.2.5}
%{!?dovecot_min_version: %define dovecot_version 2.2.21}

%if 0%{?suse_version} || 0%{?sle_version}
 %{!?libjansson_devel: %define libjansson_devel libjansson-devel}
 %{!?pkg_config: %define pkg_config pkg-config}
%endif

# openSUSE Tumbleweed
%if 0%{?suse_version} > 1500
 %{!?dovecot_devel: %define dovecot_devel dovecot23-devel}
%else
 %if 0%{?sle_version}
  # openSUSE Leap 42.3 and SLE 12 SP3
  %if 0%{?sle_version} == 120300
   %{!?dovecot_devel: %define dovecot_devel dovecot22-devel}
  %else
   %{!?dovecot_devel: %define dovecot_devel dovecot23-devel}
  %endif
 %else
  %if 0%{?centos_version} == 700
   %{!?dovecot_devel: %define dovecot_devel dovecot22u-devel}
   %{!?libjansson_devel: %define libjansson_devel jansson-devel}
   %{!?pkg_config: %define pkg_config pkgconfig}
  %endif
 %endif
%endif

Name:		dovecot-ceph-plugin
Summary:	Dovecot Ceph RADOS plugins
Version:	0.0.21
Release:	0%{?dist}
URL:		https://github.com/ceph-dovecot/dovecot-ceph-plugin
Group:		Productivity/Networking/Email/Servers
License:	LGPL-2.1
Source:		%{name}-%{version}.tar.xz

Provides:	dovecot-ceph-plugin = %{version}-%{release}
Requires:	librmb0 >= %{version}-%{release}
Requires:	librados2 >= %{librados_version}
Conflicts:	otherproviders(dovecot-ceph-plugin)


BuildRoot:      %{_tmppath}/%{name}-%{version}-build
BuildRequires:  %{dovecot_devel} >= %{dovecot_min_version}
BuildRequires:  librados-devel >= %{librados_version}
BuildRequires:  %{libjansson_devel} >= 2.9
BuildRequires:  gcc-c++
BuildRequires:  libtool
BuildRequires:  %{pkg_config}

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
	--with-dovecot=%{_libdir}/dovecot
%{__make}

%install
%{__rm} -rf %{buildroot}
mkdir -p %{buildroot}%{_libdir}/
%make_install
install -m 0644 -D COPYING-LGPL2.1 %{buildroot}%{_docdir}/dovecot-ceph-plugin/COPYING

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
%dir %{_docdir}/dovecot-ceph-plugin
%doc %{_docdir}/dovecot-ceph-plugin/COPYING
%dir %{_libdir}/dovecot
%{_libdir}/dovecot/lib*.so*
%dir %{_libdir}/dovecot/doveadm
%{_libdir}/dovecot/doveadm/lib10_doveadm_rbox_plugin.so

%files -n librmb0
%defattr(-,root,root)
%{_libdir}/librmb.so.*

%post -n librmb0 -p /sbin/ldconfig

%postun -n librmb0 -p /sbin/ldconfig

%files -n librmb-devel
%defattr(-,root,root)
%dir %{_includedir}/dovecot-ceph-plugin
%{_includedir}/dovecot-ceph-plugin/*.h
%{_libdir}/librmb.so

%files -n rmb-tools
%defattr(-,root,root)
%{_bindir}/rmb
/usr/share/man/man1/rmb.1.gz

%changelog
