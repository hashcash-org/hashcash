Summary: Hashcash anti-spam / denial-of-service counter-measure tool
Name: hashcash
Version: 1.04
Release: 1
License: CPL or choice of public domain/BSD/LGPL/GPL
Group: Development/Tools
URL: http://www.hashcash.org/
Source: http://www.hashcash.org/binaries/rpms/%{name}-%{version}.tgz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot

%description
Hashcash is a denial-of-service counter measure tool.  It's main current use
is to help hashcash users avoid losing email due to content based and
blacklist based anti-spam systems.

The hashcash tool allows you to create hashcash stamp to attach to emails
you send, and to verify hashcash stamp attached to emails you receive. 
Email senders attach hashcash stamps with the X-Hashcash: header.  Vendors
and authors of anti-spam tools are encouraged to exempt mail sent with
hashcash from their blacklists and content based filtering rules.

A hashcash stamp constitutes a proof-of-work which takes a parameterizable
amount of work to compute for the sender.  The recipient can verify received
stamps efficiently. This package also includes a sha1 implementation which
behaves somewhat like md5sum, but with SHA1.

%prep
%setup -q

%build
%ifarch i386
make COPT="$RPM_OPT_FLAGS" "PACKAGER=RPM" x86
%else
%ifarch ppc
make COPT="$RPM_OPT_FLAGS" "PACKAGER=RPM" ppc-linux
%else
make COPT="$RPM_OPT_FLAGS" "PACKAGER=RPM" 
%endif
%endif


%install
rm -rf $RPM_BUILD_ROOT
install -d $RPM_BUILD_ROOT/%{_bindir}/
install -d $RPM_BUILD_ROOT/%{_mandir}
install -d $RPM_BUILD_ROOT/%{_mandir}/man1
install -d $RPM_BUILD_ROOT/%{_docdir}/
install -d $RPM_BUILD_ROOT/%{_docdir}/%{name}-%{version}

install -m 755 hashcash $RPM_BUILD_ROOT/%{_bindir}/
install -m 755 sha1 $RPM_BUILD_ROOT/%{_bindir}/
install -m 644 hashcash.1 $RPM_BUILD_ROOT/%{_mandir}/man1
install -m 644 sha1-hashcash.1 $RPM_BUILD_ROOT/%{_mandir}/man1
install -m 644 README LICENSE CHANGELOG $RPM_BUILD_ROOT/%{_docdir}/%{name}-%{version}/

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,-)

%{_bindir}/*
%{_mandir}/*/*
%{_docdir}/*

%changelog
* Wed Sep 07 2004 Adam Back <adam@cypherspace.org>
- add cpu/platform specific tests to compile best code for platform
* Sun Mar 07 2004 Adam Back <adam@cypherspace.org>
- used general targets {_bin|_man|_doc}dir etc
* Sun Mar 07 2004 Jochen Schönfelder <arisel@arisel.de>
- tried to merge Mandrake & redhat rpms
* Sat Mar 06 2004 Jochen Schönfelder <arisel@arisel.de>
- Mandrake-build fixes
- sha1-manpage moved to sha1-hashcash 
* Thu Jun 26 2003 Adam Back <adam@cypherspace.org> 
- First spec file
