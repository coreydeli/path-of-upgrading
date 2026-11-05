#!/bin/sh
# SPDX-License-Identifier: MIT
#
# What building, testing and packaging needs on Windows, in MSYS2's UCRT64
# shell. The Plasma style and Breeze's icons go in the app, LuaJIT runs Path
# of Building, and NSIS makes the installer.
set -eu
p=mingw-w64-ucrt-x86_64
pacman -S --noconfirm --needed \
    zip \
    $p-gcc $p-cmake $p-ninja $p-pkgconf $p-ccache \
    $p-qt6-base $p-qt6-declarative $p-qt6-svg \
    $p-extra-cmake-modules $p-kirigami $p-kirigami-addons $p-kdeclarative $p-ki18n $p-kcolorscheme \
    $p-kstatusnotifieritem $p-kiconthemes $p-qqc2-desktop-style $p-breeze-icons \
    $p-qtkeychain-qt6 $p-zlib $p-luajit $p-nsis
