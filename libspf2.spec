# TODO
# ./configure[3000]: AX_WITH_PERL: not found - needed after regeneration
#
# Conditional build:
%bcond_without	tests	# PLD regression and upstream parser tests

Summary:	Implementation of the SPF specification
Summary(pl.UTF-8):	Implementacja specyfikacji SPF
Name:		libspf2
Version:	1.2.11
Release:	4
License:	LGPL
Group:		Libraries
# Source0:	http://www.libspf2.org/spf/%{name}-%{version}.tar.gz
Source0:	https://github.com/shevek/libspf2/tarball/4915c30#/%{name}.tar.gz
# Source0-md5:	ad5f1b48e4f2b5561df99ca7cbf30e92
Source1:	%{name}-tests.c
Patch0:		%{name}-link.patch
Patch1:		0001-remove-libreplace-unneeded-on-Linux.patch
# CVE-2023-42118; https://github.com/shevek/libspf2/pull/44
Patch2:         integer-underflow.patch
Patch3:         spf_dns-include-std-headers.patch
Patch4:         abolish-spf-rrtype.patch
Patch5:		record-mech-len.patch
# https://bugs.debian.org/1065792
Patch6:		fix-include.patch
# https://github.com/shevek/libspf2/pull/10
Patch7:		redirect-last.patch
Patch8:		macro-truncate.patch
# https://github.com/shevek/libspf2/pull/25
Patch9:		dns-resolv-num-rr.patch
# https://github.com/shevek/libspf2/pull/15
Patch10:	query-record-uaf.patch
# https://github.com/shevek/libspf2/pull/37
Patch11:	dns-cache-precedence.patch
Patch12:	rfc-error-results.patch
Patch13:	duplicate-exp.patch
Patch14:	long-values.patch
Patch15:	domain-truncate.patch
Patch16:	empty-domain-spec.patch
Patch17:	dns-mech-limit.patch
Patch18:	malformed-domain.patch
URL:		https://github.com/shevek/libspf2
BuildRequires:	autoconf >= 2.59
BuildRequires:	automake
BuildRequires:	libtool
%if %{with tests}
BuildRequires:	perl(Test::More)
BuildRequires:	perl(Text::ParseWords)
BuildRequires:	perl-base
%endif
Obsoletes:	libspf_alt
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%description
Libspf2 is an implementation of the SPF specification as found at
<http://www.ietf.org/internet-drafts/draft-mengwong-spf-01.txt>.

%description -l pl.UTF-8
Libspf2 jest implementacją specyfikacji SPF, która znajduje się pod
<adresem
http://www.ietf.org/internet-drafts/draft-mengwong-spf-01.txt>.

%package tools
Summary:	Tools distributed with libspf2
Summary(pl.UTF-8):	Programy narzędziowe, dystrybuowane z libspf2
Group:		Networking/Utilities
Requires:	%{name} = %{version}-%{release}

%description tools
Tools distributed with libspf2; at the time of writing: spf_example,
spf_example_2mx, spfd, spfquery and spftest.

%description tools -l pl.UTF-8
Programy narzędziowe, dystrybuowane z libspf2; w momencie pisania tego
opisu: spf_example, spf_example_2mx, spfd, spfquery i spftest.

%package devel
Summary:	Header files for libspf2 library
Summary(pl.UTF-8):	Pliki nagłówkowe biblioteki libspf2
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}
Obsoletes:	libspf_alt-devel

%description devel
Header files for libspf2 library.

%description devel -l pl.UTF-8
Pliki nagłówkowe biblioteki libspf2.

%package static
Summary:	Static libspf2 library
Summary(pl.UTF-8):	Statyczna biblioteka libspf2
Group:		Development/Libraries
Requires:	%{name}-devel = %{version}-%{release}
Obsoletes:	libspf_alt-static

%description static
Static libspf2 library.

%description static -l pl.UTF-8
Statyczna biblioteka libspf2.

%prep
#%%setup -q
%setup -q -n shevek-libspf2-4915c30
%patch -P0 -p1
%patch -P1 -p1
%patch -P2 -p1
%patch -P3 -p1
%patch -P4 -p1
%patch -P5 -p1
%patch -P6 -p1
%patch -P7 -p1
%patch -P8 -p1
%patch -P9 -p1
%patch -P10 -p1
%patch -P11 -p1
%patch -P12 -p1
%patch -P13 -p1
%patch -P14 -p1
%patch -P15 -p1
%patch -P16 -p1
%patch -P17 -p1
%patch -P18 -p1

%build
%{__libtoolize}
%{__aclocal}
%{__autoconf}
%{__autoheader}
%{__automake}
%configure
%{__make}

%if %{with tests}
%{__cc} %{rpmcflags} %{rpmcppflags} %{rpmldflags} -Isrc/include -I. \
	-o libspf2-tests %{SOURCE1} src/libspf2/.libs/libspf2.a -lresolv -lpthread
./libspf2-tests
# other upstream suites fail without PLD patches too; test_live needs network
cd tests
./run_single_parser
%endif

%install
rm -rf $RPM_BUILD_ROOT
%{__make} install \
	DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%post	-p /sbin/ldconfig
%postun	-p /sbin/ldconfig

%files
%defattr(644,root,root,755)
%doc README TODO LICENSES INSTALL
%attr(755,root,root) %{_libdir}/libspf2.so.*.*.*
%ghost %{_libdir}/libspf2.so.2

%files tools
%defattr(644,root,root,755)
%attr(755,root,root) %{_bindir}/spf_example
%attr(755,root,root) %{_bindir}/spf_example_static
%attr(755,root,root) %{_bindir}/spfd
%attr(755,root,root) %{_bindir}/spfd_static
%attr(755,root,root) %{_bindir}/spfquery
%attr(755,root,root) %{_bindir}/spfquery_static
%attr(755,root,root) %{_bindir}/spftest
%attr(755,root,root) %{_bindir}/spftest_static

%files devel
%defattr(644,root,root,755)
%attr(755,root,root) %{_libdir}/libspf2.so
%{_libdir}/libspf2.la
%dir %{_includedir}/spf2
%{_includedir}/spf2/*.h

%files static
%defattr(644,root,root,755)
%{_libdir}/libspf2.a
