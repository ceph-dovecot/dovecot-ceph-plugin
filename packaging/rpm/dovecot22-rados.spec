#
# spec file for package dovecot22-rados-plugins
#
# Copyright (c) 2017 Tallence AG and the authors
#
# This is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License version 2.1, as published by the Free Software
# Foundation.  See file COPYING.

Name:          	dovecot22-rados-plugins
Summary:       	Dovecot RADOS plugins
Version:       	0.0.1
Release:       	1

Group:          Productivity/Networking/Email/Servers
License:        LGPL-2.1

Source:        	%{name}_%{version}-%{release}.tar.gz

%define dovecot_home      /usr
%define librados_version  10.2.5

Provides:       dovecot-rados-plugins = %{version}-%{release}
Requires: 		librados2 >= %{librados_version}
Conflicts:      otherproviders(dovecot-rados-plugins)

BuildRoot:		%{_tmppath}/%{name}-%{version}-build
BuildRequires: 	dovecot22-devel
BuildRequires:  librados-devel >= %{librados_version}
BuildRequires:  gcc-c++
BuildRequires:  libtool

%description
Dovecot is an IMAP and POP3 server for Linux and UNIX-like systems,
written primarily with security in mind. Although it is written in C,
it uses several coding techniques to avoid most of the common pitfalls.

Dovecot can work with standard mbox and maildir formats and is fully
compatible with UW-IMAP and Courier IMAP servers as well as mail
clients accessing the mailboxes directly.

This package holds the files needed for RADOS support.

%prep
%setup -q

%build
export CFLAGS="%{optflags}"
export CFLAGS="$CFLAGS -fpic -DPIC"
export LIBS="-pie"

./autogen.sh
%configure \
	--prefix=%{dovecot_home} \
	--with-dovecot=%{dovecot_home}/lib64/dovecot
%{__make}

%install
%makeinstall

# clean up unused files
find %{buildroot}%{dovecot_home}/lib64/dovecot/ -type f -name \*.la -delete
find %{buildroot}%{dovecot_home}/lib64/dovecot/ -type f -name \*.a  -delete
find %{buildroot}%{dovecot_home}/ -type f -name \*.h  -delete

%clean
%{__rm} -rf %{buildroot}

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
%defattr(-,root,root)

#%dir /opt/app/
%dir %{dovecot_home}/
%dir %{dovecot_home}/lib64/
%dir %{dovecot_home}/lib64/dovecot
%{dovecot_home}/lib64/dovecot/lib*.so*
#%{dovecot_home}/lib64
%doc HISTORY

%changelog
