# SPDX-License-Identifier: MIT
#
# The release builds put the tarball from git archive in the sources folder,
# unpacking to the release's folder (a beta's too), and replace Version with
# the one the release says. A beta's is like 0.7.0~beta.1, which sorts before
# 0.7.0. Its Source0 only points at the tag when the version is a release.

# a warning fails the build with --with werror
%bcond werror 0
# the tests need an X server and a bus, so --without check skips them
%bcond check 1

Name:           path-of-upgrading
Version:        0.7.0
Release:        1%{?dist}
# the release a beta leads up to, whose folder the tarball unpacks to
%global srcver  %(echo '%{version}' | cut -d '~' -f 1)
Summary:        Path of Exile 2 item overlay that checks items against your build
License:        MIT AND OFL-1.1
URL:            https://github.com/coreydeli/path-of-upgrading
Source0:        %{url}/archive/v%{version}.tar.gz#/%{name}-%{version}.tar.gz

BuildRequires:  cmake >= 3.21
BuildRequires:  ninja-build
BuildRequires:  gcc-c++
BuildRequires:  pkgconfig
BuildRequires:  cmake(Qt6Core) >= 6.8
BuildRequires:  cmake(Qt6Gui)
BuildRequires:  cmake(Qt6Network)
BuildRequires:  cmake(Qt6Widgets)
BuildRequires:  cmake(Qt6Qml)
BuildRequires:  cmake(Qt6Quick)
BuildRequires:  cmake(Qt6QuickControls2)
BuildRequires:  cmake(Qt6DBus)
BuildRequires:  cmake(Qt6Test)
BuildRequires:  cmake(Qt6QuickTest)
BuildRequires:  cmake(Qt6Keychain)
BuildRequires:  cmake(KF6Kirigami) >= 6.13
BuildRequires:  cmake(KF6KirigamiAddons) >= 1.7
BuildRequires:  cmake(KF6Declarative) >= 6.13
BuildRequires:  cmake(KF6ColorScheme) >= 6.13
BuildRequires:  cmake(KF6I18n) >= 6.13
BuildRequires:  cmake(KF6Config)
BuildRequires:  cmake(KF6DBusAddons)
BuildRequires:  cmake(KF6GlobalAccel)
BuildRequires:  cmake(KF6Notifications)
BuildRequires:  cmake(KF6StatusNotifierItem)
BuildRequires:  cmake(KF6WindowSystem)
BuildRequires:  cmake(LayerShellQt)
BuildRequires:  pkgconfig(xcb)
BuildRequires:  pkgconfig(xcb-keysyms)
BuildRequires:  pkgconfig(xcb-randr)
BuildRequires:  pkgconfig(xcb-xfixes)
BuildRequires:  pkgconfig(xcb-xtest)
BuildRequires:  pkgconfig(zlib)
BuildRequires:  desktop-file-utils
BuildRequires:  libappstream-glib
BuildRequires:  appstream
%if %{with check}
# LuaJIT runs the bridge to Path of Building in its test
BuildRequires:  dbus-daemon
BuildRequires:  luajit
BuildRequires:  xorg-x11-server-Xvfb
BuildRequires:  xorg-x11-xauth
%endif

# The program only loads the QML modules when it runs, so the libraries it
# links don't bring them in.
Requires:       hicolor-icon-theme
Requires:       qt6-qtdeclarative
Requires:       qt6-qtwayland
# the Wayland plugin the overlay sits over the game with
Requires:       layer-shell-qt
Requires:       kf6-kirigami
Requires:       kf6-kirigami-addons
Requires:       kf6-kdeclarative
# the Plasma style for the window, and Path of Building from Flathub
Recommends:     kf6-qqc2-desktop-style
Recommends:     flatpak
# to run another install of Path of Building
Suggests:       luajit

%description
Press a hotkey over an item in Path of Exile 2 and a popup says whether it's
an upgrade for your build, by gear score, by your stat weights, or by Path of
Building once a build is imported. It can search the trade site for upgrades,
price an item and find bases to craft on.

%prep
%autosetup -n %{name}-%{srcver}

%build
# Qt adds a run path to the program, which it doesn't need in /usr/bin.
# Trade searches start on in a beta, and off in a release.
%cmake -G Ninja \
    -DCMAKE_SKIP_INSTALL_RPATH=ON \
    -DCMAKE_COMPILE_WARNING_AS_ERROR=%{?with_werror:ON}%{!?with_werror:OFF} \
    -DBUILD_TESTING=%{?with_check:ON}%{!?with_check:OFF} \
    -DPOU_BETA=%[ "%{version}" != "%{srcver}" ? "ON" : "OFF" ]
%cmake_build

%install
%cmake_install

%if %{with check}
%check
desktop-file-validate %{buildroot}%{_datadir}/applications/io.github.coreydeli.PathOfUpgrading.desktop
cd %{__cmake_builddir}
sh ../.github/scripts/run-tests.sh
%endif

%files
%license LICENSE
%doc README.md
%license %{_datadir}/licenses/%{name}/OFL-Cinzel.txt
%{_bindir}/%{name}
%{_datadir}/applications/io.github.coreydeli.PathOfUpgrading.desktop
%{_datadir}/icons/hicolor/scalable/apps/io.github.coreydeli.PathOfUpgrading.svg
%{_datadir}/knotifications6/%{name}.notifyrc
%{_datadir}/metainfo/io.github.coreydeli.PathOfUpgrading.metainfo.xml
%{_datadir}/%{name}/

%changelog
* Fri Nov 06 2026 Corey Deli <55706677+coreydeli@users.noreply.github.com> - 0.5.0-1
- Package for Fedora
