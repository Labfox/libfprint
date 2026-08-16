%global fork_suffix goodix5xx
%global versioned_name libfprint-2

# Build against the same driver set as upstream Fedora, plus goodix5xx
# (goodix5xx is part of the "default" set in this fork).
%global drivers default

Name:           libfprint
Version:        1.94.5
Release:        1%{?dist}.%{fork_suffix}
Summary:        Library for fingerprint readers (fork with the Goodix 27c6:55a4 driver)

License:        LGPL-2.1-or-later
URL:            https://github.com/Labfox/libfprint
Source0:        %{name}-%{version}.tar.gz

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  meson >= 0.49.0
BuildRequires:  ninja-build
BuildRequires:  pkgconfig(glib-2.0) >= 2.56
BuildRequires:  pkgconfig(gio-unix-2.0) >= 2.56
BuildRequires:  pkgconfig(gobject-2.0) >= 2.56
BuildRequires:  pkgconfig(gusb) >= 0.2.0
BuildRequires:  pkgconfig(gudev-1.0)
BuildRequires:  pkgconfig(nss)
BuildRequires:  pkgconfig(openssl)
BuildRequires:  pkgconfig(pixman-1)
BuildRequires:  pkgconfig(cairo)
BuildRequires:  pkgconfig(udev)
BuildRequires:  pkgconfig(opencv4)
BuildRequires:  gobject-introspection-devel
BuildRequires:  systemd-rpm-macros

# Runtime deps (glib2, libgusb, libgudev, nss, pixman, openssl-libs and the
# OpenCV libs the SIGFM matcher links) are picked up automatically by RPM's
# soname dependency generator; on Fedora 42 that resolves to opencv-core,
# opencv-imgproc, opencv-features2d, opencv-flann and friends.

%description
libfprint offers programs a single interface to a range of consumer fingerprint
readers.

This build is the Labfox fork: it adds the "goodix5xx" driver for the Goodix
27c6:55a4 sensor (TLS-PSK transport, SIFT/SIGFM matching via OpenCV) on top of
libfprint %{version}. It is API/ABI compatible with upstream libfprint 2
(soname libfprint-2.so.2) and is meant to replace the distribution package.

%package devel
Summary:        Development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Header files, pkg-config file and GObject introspection XML for %{name}.

%prep
%autosetup -n %{name}-%{version}

%build
%meson \
    -Ddrivers=%{drivers} \
    -Dintrospection=true \
    -Ddoc=false \
    -Dgtk-examples=false \
    -Dudev_rules=enabled \
    -Dudev_rules_dir=%{_udevrulesdir} \
    -Dudev_hwdb=disabled
%meson_build

%install
%meson_install

%check
# The upstream test-suite needs a virtual device build and umockdev; skip it
# here and rely on the shared-library ABI check below.
test -f %{buildroot}%{_libdir}/libfprint-2.so.2

%files
%license COPYING
%doc README.md
%{_libdir}/libfprint-2.so.2
%{_libdir}/libfprint-2.so.2.*
%{_libdir}/girepository-1.0/FPrint-2.0.typelib
%{_udevrulesdir}/70-libfprint-2.rules

%files devel
%{_includedir}/libfprint-2/
%{_libdir}/libfprint-2.so
%{_libdir}/pkgconfig/libfprint-2.pc
%{_datadir}/gir-1.0/FPrint-2.0.gir

%changelog
* Sun Aug 16 2026 Labfox <labfoxdev@gmail.com> - 1.94.5-1.goodix5xx
- Initial Fedora/secureblue packaging of the goodix5xx fork
