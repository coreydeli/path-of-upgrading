#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Installs a package in a container with nothing else, as root, with the
# distribution's own tool, to see that it finds everything the package needs,
# brings the Wayland overlay plugin, and starts. It runs until the timeout,
# unless a QML module is missing, which stops it at once.
#
# check-install.sh <arch, debian or fedora> <the package file>
set -eu
# Arch's mirrors drop a download now and then, so pacman gets three tries
pacman_retry() {
    for try in 1 2 3; do
        pacman "$@" && return 0
        [ "$try" = 3 ] || sleep 15
    done
    return 1
}

distro=$1
package=$(realpath "$2")

case "$distro" in
arch)
    pacman_retry -Syu --noconfirm
    pacman_retry -U --noconfirm "$package"
    pacman_retry -S --noconfirm --needed dbus
    plugin=/usr/lib/qt6/plugins/wayland-shell-integration/liblayer-shell.so
    ;;
debian)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y "$package" dbus-daemon
    # the build's own packages could have brought it in already, so this
    # checks that the package itself asks for it
    dpkg-deb -f "$package" Depends | grep -qw layer-shell-qt
    plugin=/usr/lib/x86_64-linux-gnu/qt6/plugins/wayland-shell-integration/liblayer-shell.so
    ;;
fedora)
    dnf -y install "$package" dbus-daemon
    plugin=/usr/lib64/qt6/plugins/wayland-shell-integration/liblayer-shell.so
    ;;
*)
    echo "Don't know $distro" >&2
    exit 2
    ;;
esac

test -f "$plugin"

export QT_QPA_PLATFORM=offscreen
export QT_NO_XDG_DESKTOP_PORTAL=1
export XDG_RUNTIME_DIR=/tmp/runtime
mkdir -m 700 "$XDG_RUNTIME_DIR"

path-of-upgrading --version
status=0
timeout 10 dbus-run-session -- path-of-upgrading || status=$?
test "$status" -eq 124
