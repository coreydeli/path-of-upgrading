#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Prints the version from CMakeLists.txt, once the metainfo's newest release
# and the PKGBUILD say the same, and the tag when one is given.
set -eu
cd "$(dirname "$0")/../.."
metainfo=data/io.github.coreydeli.PathOfUpgrading.metainfo.xml
version=$(sed -n 's/^    VERSION \([0-9.]*\)$/\1/p' CMakeLists.txt)
released=$(sed -n 's/.*<release version="\([^"]*\)".*/\1/p' "$metainfo" | head -n 1)
pkgver=$(sed -n 's/^pkgver=//p' packaging/arch/PKGBUILD)
ok=true
if [ -z "$version" ]; then
    echo "::error file=CMakeLists.txt::There's no VERSION in the project() call" >&2
    exit 1
fi
if [ "$released" != "$version" ]; then
    echo "::error file=$metainfo::The newest release is $released, not $version" >&2
    ok=false
fi
if [ "$pkgver" != "$version" ]; then
    echo "::error file=packaging/arch/PKGBUILD::pkgver is $pkgver, not $version" >&2
    ok=false
fi
if [ $# -gt 0 ] && [ "$1" != "v$version" ]; then
    echo "::error::The tag is $1, but the version is $version" >&2
    ok=false
fi
$ok
echo "$version"
