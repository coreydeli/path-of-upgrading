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
    plugins=/usr/lib/qt6/plugins
    ;;
debian)
    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    # without what it recommends, so those can't stand in for a missing
    # dependency
    apt-get install -y --no-install-recommends "$package" dbus-daemon
    # the build's own packages could have brought it in already, so this
    # checks that the package itself asks for it
    dpkg-deb -f "$package" Depends | grep -qw layer-shell-qt
    # the certificates the trade site, poe.ninja and GitHub are checked by
    test -f /etc/ssl/certs/ca-certificates.crt
    plugins=/usr/lib/x86_64-linux-gnu/qt6/plugins
    ;;
fedora)
    dnf -y install "$package" dbus-daemon
    plugins=/usr/lib64/qt6/plugins
    ;;
*)
    echo "Don't know $distro" >&2
    exit 2
    ;;
esac

# the Wayland overlay, and the icons, which are SVG
test -f "$plugins/wayland-shell-integration/liblayer-shell.so"
test -f "$plugins/iconengines/libqsvgicon.so"

export QT_QPA_PLATFORM=offscreen
export QT_NO_XDG_DESKTOP_PORTAL=1
export XDG_RUNTIME_DIR=/tmp/runtime
mkdir -m 700 "$XDG_RUNTIME_DIR"

path-of-upgrading --version
status=0
timeout 10 dbus-run-session -- path-of-upgrading || status=$?
test "$status" -eq 124
