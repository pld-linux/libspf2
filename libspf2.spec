# TODO
# ./configure[3000]: AX_WITH_PERL: not found - needed after regeneration

Summary:	Implementation of the SPF specification
Summary(pl.UTF-8):	Implementacja specyfikacji SPF
Name:		libspf2
Version:	1.2.10
Release:	2
License:	LGPL
Group:		Libraries
Source0:	http://www.libspf2.org/spf/%{name}-%{version}.tar.gz
# Source0-md5:	7bb9937d0705649eaa8646de66dc3562
Patch0:		%{name}-link.patch
URL:		http://www.libspf2.org/
BuildRequires:	autoconf >= 2.59
BuildRequires:	automake
BuildRequires:	libtool
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
%setup -q
%patch0 -p1

%build
%{__libtoolize}
%{__aclocal}
%{__autoconf}
%{__autoheader}
%{__automake}
%configure
%{__make}

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
