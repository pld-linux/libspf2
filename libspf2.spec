Summary:	Implementation of the SPF specification
Summary(pl):	Implementacja specyfikacji SPF
Name:		libspf2
Version:	1.0.4
Release:	0.1
License:	LGPL
Group:		Libraries
Source0:	http://libspf2.org/spf/%{name}-%{version}.tar.gz
# Source0-md5:	5fe69ba13bf35d505b733247032a8a64
BuildRequires:	autoconf >= 2.59
BuildRequires:	automake
BuildRequires:	libtool
#Obsoletes:	libspf_alt
BuildRoot:	%{tmpdir}/%{name}-%{version}-root-%(id -u -n)

%description
Libspf2 is an implementation of the SPF specification as found at
http://www.ietf.org/internet-drafts/draft-mengwong-spf-01.txt .

%description -l pl
Libspf2 jest implementacj± specyfikacji SPF, która znajduje siê pod
adresem
http://www.ietf.org/internet-drafts/draft-mengwong-spf-01.txt .

%package devel
Summary:	Header files for libspf2 library
Summary(pl):	Pliki nag³ówkowe biblioteki libspf2
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%description devel
Header files for libspf2 library.

%description devel -l pl
Pliki nag³ówkowe biblioteki libspf2.

%package static
Summary:	Static libspf2 library
Summary(pl):	Statyczna biblioteka libspf2
Group:		Development/Libraries
Requires:	%{name}-devel = %{version}-%{release}

%description static
Static libspf2 library.

%description static -l pl
Statyczna biblioteka libspf2.

%prep
%setup -q

%build
%{__libtoolize}
%{__aclocal}
%{__autoconf}
%{__autoheader}
%{__automake} 
cp -f /usr/share/automake/config.sub .
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
%doc README TODO LICENSES INSTALL Changelog docs/API docs/draft-mengwong-spf-00.txt
%attr(755,root,root) %{_bindir}/spf_example
%attr(755,root,root) %{_bindir}/spf_example_2mx
%attr(755,root,root) %{_bindir}/spfd
%attr(755,root,root) %{_bindir}/spfquery
%attr(755,root,root) %{_bindir}/spftest
%attr(755,root,root) %{_libdir}/lib*.so.*.*.*

%files devel
%defattr(644,root,root,755)
%attr(755,root,root) %{_libdir}/lib*.so
%{_libdir}/lib*.la
%dir %{_includedir}/spf2
%{_includedir}/spf2/*.h

%files static
%defattr(644,root,root,755)
%{_libdir}/lib*.a
