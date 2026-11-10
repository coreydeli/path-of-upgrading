#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Makes the AppImage from a release build, as root on Debian 13 in the CI's
# container, which is the oldest system the app supports, as the AppImage
# runs where its glibc is as new as the one it was built with. linuxdeploy
# and its Qt plugin collect the libraries and the QML modules the program
# imports. The modules it can't see (the Plasma style, which Qt picks by name
# when it runs) and the plugins the KDE libraries load are added here.
#
# package-appimage.sh <build folder> <version, like 0.1.0 or 0.1.0-beta.2>
# It needs curl, binutils, file, qt6-wayland, qt6-svg-plugins and layer-shell-qt installed.
# It makes PathOfUpgrading-<version>-x86_64.AppImage in the folder it's run from.
set -eu
build=$1
version=$2
repo=$(cd "$(dirname "$0")/../.." && pwd)
out=$PWD

# The tools are pinned, and untrusted until their checksums match.
linuxdeploy_url=https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage
linuxdeploy_sum=c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d
qtplugin_url=https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage
qtplugin_sum=15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724

tools=$(mktemp -d)
fetch() {
    curl -fsSL --retry 3 -o "$tools/$1" "$2"
    echo "$3  $tools/$1" | sha256sum -c -
    chmod +x "$tools/$1"
}
fetch linuxdeploy-x86_64.AppImage "$linuxdeploy_url" "$linuxdeploy_sum"
fetch linuxdeploy-plugin-qt-x86_64.AppImage "$qtplugin_url" "$qtplugin_sum"

lib=/usr/lib/x86_64-linux-gnu
appdir=$out/AppDir
rm -rf "$appdir"
DESTDIR=$appdir cmake --install "$build" --prefix /usr --strip

export QMAKE=/usr/lib/qt6/bin/qmake6
export QML_SOURCES_PATHS="$repo/src/app/qml"
export EXTRA_PLATFORM_PLUGINS="libqwayland-generic.so;libqwayland-egl.so;libqoffscreen.so"
export LD_LIBRARY_PATH="$lib:${LD_LIBRARY_PATH:-}"
export LINUXDEPLOY_OUTPUT_VERSION="$version"
export VERSION="$version"
# so none of the tools needs FUSE in the container
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="$tools:$PATH"

cd "$out"
desktop=$appdir/usr/share/applications/io.github.coreydeli.PathOfUpgrading.desktop
icon=$appdir/usr/share/icons/hicolor/scalable/apps/io.github.coreydeli.PathOfUpgrading.svg
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --plugin qt \
    --desktop-file "$desktop" --icon-file "$icon"

# What the KDE libraries load by name when they run. Kirigami's style comes
# from the Plasma style when the platform theme asks for it, and from Qt's
# when it doesn't.
mkdir -p "$appdir/usr/lib/qt6/qml/org/kde" "$appdir/usr/lib/qt6/plugins"
cp -r "$lib/qt6/qml/org/kde/desktop" "$lib/qt6/qml/org/kde/qqc2desktopstyle" \
    "$appdir/usr/lib/qt6/qml/org/kde/"
cp -r "$lib/qt6/plugins/kf6" "$appdir/usr/lib/qt6/plugins/"
# The Wayland plugins, with LayerShellQt's shell integration for the overlay
cp -r "$lib"/qt6/plugins/wayland-* "$appdir/usr/lib/qt6/plugins/"

# The notification settings are found through the data folders, which are the
# system's unless the AppImage adds its own.
mkdir -p "$appdir/apprun-hooks"
cat > "$appdir/apprun-hooks/path-of-upgrading-hook.sh" << 'HOOK'
export XDG_DATA_DIRS="$APPDIR/usr/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}"
HOOK

# Again, to bring in what those need, and to make the image. They come after
# the Qt plugin as it would read the Plasma style's QML for styles that
# aren't there.
"$tools/linuxdeploy-x86_64.AppImage" --appdir "$appdir" --output appimage \
    --desktop-file "$desktop" --icon-file "$icon"

image=$(ls Path*of*Upgrading*.AppImage)
mv "$image" "PathOfUpgrading-$version-x86_64.AppImage"
rm -rf "$tools"
