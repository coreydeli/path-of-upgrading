#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Starts an AppImage in a container with nothing else, as root, on Fedora or
# Arch Linux. The AppImage leaves out what a desktop already has (the X and
# OpenGL libraries, the font stack), so those are installed first, as they
# would be anywhere you'd run it. It runs until the timeout, unless a QML
# module or a plugin is missing, which stops it at once.
#
# check-appimage.sh <the AppImage>
set -eu
image=$(realpath "$1")
if command -v dnf > /dev/null; then
    dnf -y install dbus-daemon libxcb libglvnd-egl libglvnd-glx libglvnd-opengl harfbuzz freetype \
        libxkbcommon fontconfig
else
    pacman -Sy --noconfirm dbus libxcb libglvnd harfbuzz freetype2 libxkbcommon fontconfig
fi
chmod +x "$image"

# there's no FUSE in a container
export APPIMAGE_EXTRACT_AND_RUN=1
export QT_QPA_PLATFORM=offscreen
export QT_NO_XDG_DESKTOP_PORTAL=1
export XDG_RUNTIME_DIR=/tmp/runtime
mkdir -m 700 "$XDG_RUNTIME_DIR"

"$image" --version
status=0
timeout 10 dbus-run-session -- "$image" || status=$?
test "$status" -eq 124
