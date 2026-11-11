#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Makes the Windows installer and zip from a release build, in MSYS2's UCRT64
# shell. The app's folder has the program with every library it loads from
# MSYS2, the QML modules and plugins, Breeze's icons, and LuaJIT for Path of
# Building.
#
# package-windows.sh <build folder> <version, like 0.1.0> <what the files are called after>
set -eu
build=$1
version=$2
label=$3
repo=$(cd "$(dirname "$0")/../.." && pwd)
out=$PWD
app="$out/Path of Upgrading"

# Qt's own folders, which MSYS2's tools may have -qt6 on their names
qtpaths=$(command -v qtpaths-qt6 || command -v qtpaths6 || command -v qtpaths)
qml=$("$qtpaths" --query QT_INSTALL_QML)
plugins=$("$qtpaths" --query QT_INSTALL_PLUGINS)
echo "QML from $qml and plugins from $plugins"

mkdir "$app"
# which qt_standard_project_setup puts at the top of the build folder on Windows
cp "$build/path-of-upgrading.exe" "$app/"
cp /ucrt64/bin/luajit.exe "$app/"
cp -r "$qml" "$app/qml"
cp -r "$plugins"/. "$app/"
# the app's own style, for Kirigami
mkdir -p "$app/kf6/kirigami/platform"
cp "$build/src/kf6/kirigami/platform/PouStyle.dll" "$app/kf6/kirigami/platform/"
mkdir "$app/data"
cp /ucrt64/bin/data/icontheme.rcc "$app/data/"
# where the app looks for its colors, as Qt's data folders include data next to the program
mkdir "$app/data/color-schemes"
cp "$repo/data/color-schemes/PathOfUpgrading.colors" "$app/data/color-schemes/"
# where Qt looks for the plugins and the QML, as it would look in MSYS2's
cat > "$app/qt.conf" << 'CONF'
[Paths]
Prefix = .
Plugins = .
QmlImports = qml
CONF

# the libraries all of it loads, until there are no more
while :; do
    added=false
    for dll in $(find "$app" -name '*.exe' -o -name '*.dll' | while read -r file; do ldd "$file"; done |
        sed -n 's|.* => \(/ucrt64/bin/[^ ]*\) .*|\1|p' | sort -u); do
        if [ ! -e "$app/${dll##*/}" ]; then
            cp "$dll" "$app/"
            added=true
        fi
    done
    $added || break
done
if ldd "$app/path-of-upgrading.exe" | grep -i 'not found'; then
    echo "::error::The program is missing a library" >&2
    exit 1
fi

cp "$repo/LICENSE" "$app/LICENSE.txt"
cp "$repo/data/fonts/OFL-Spectral.txt" "$repo/data/fonts/OFL-BigShoulders.txt" "$app/"
zip -qr "path-of-upgrading-$label-windows.zip" "${app##*/}"
makensis -V2 -DVERSION="$version" -DLABEL="$label" -DAPP="$(cygpath -w "$app")" \
    -DOUTFILE="$(cygpath -w "$out/path-of-upgrading-$label-setup.exe")" \
    "$(cygpath -w "$repo/packaging/windows/installer.nsi")"
