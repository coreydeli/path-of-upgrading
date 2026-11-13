#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Starts an AppImage in a container with nothing else, as root, on Fedora,
# Arch Linux or Ubuntu. The AppImage leaves out what a desktop already has
# (the X and OpenGL libraries, the font stack), so those are installed first,
# as they would be anywhere you'd run it. It runs until the timeout, unless a QML
# module or a plugin is missing, which stops it at once.
#
# check-appimage.sh <the AppImage>
set -eu
image=$(realpath "$1")
if command -v dnf > /dev/null; then
    dnf -y install dbus-daemon libxcb libglvnd-egl libglvnd-glx libglvnd-opengl harfbuzz freetype \
        libxkbcommon fontconfig
elif command -v apt-get > /dev/null; then
    export DEBIAN_FRONTEND=noninteractive
    # GitHub's runners now and then can't look up Ubuntu's own servers, so after
    # a failed try, use the mirror the runners themselves use
    sources=/etc/apt/sources.list.d/ubuntu.sources
    for try in 1 2 3; do
        apt-get update --error-on=any && break
        test "$try" -lt 3 || exit 1
        sed -i 's|http://archive.ubuntu.com|http://azure.archive.ubuntu.com|g; s|http://security.ubuntu.com|http://azure.archive.ubuntu.com|g' "$sources"
        sleep 5
    done
    apt-get install -y dbus-daemon libxcb1 libegl1 libgl1 libopengl0 libharfbuzz0b libfreetype6 libxkbcommon0 \
        fontconfig
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
