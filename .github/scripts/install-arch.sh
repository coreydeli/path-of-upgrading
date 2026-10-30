#!/bin/sh
# SPDX-License-Identifier: MIT
#
# What building, testing and packaging needs on Arch Linux, as root in the
# CI's container. Xvfb is for the tests that read an X server, and the Plasma
# style is what the window tests load when it's there.
set -eu
pacman -Syu --noconfirm --needed \
    base-devel cmake ninja git \
    qt6-base qt6-declarative \
    kirigami kirigami-addons kdeclarative ki18n kdbusaddons kconfig \
    kglobalaccel knotifications kstatusnotifieritem kwindowsystem \
    layer-shell-qt qtkeychain-qt6 libxcb xcb-util-keysyms zlib \
    qqc2-desktop-style qt6-wayland hicolor-icon-theme \
    desktop-file-utils appstream dbus xorg-server-xvfb xorg-xauth
