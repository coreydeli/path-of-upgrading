#!/bin/sh
# SPDX-License-Identifier: MIT
#
# What building, testing and packaging needs on Debian 13, as root in the
# CI's container. Debian splits the QML modules out of the libraries, and the
# app only loads them when it runs, so they're named here too.
set -eu
export DEBIAN_FRONTEND=noninteractive
apt-get update
apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build pkgconf git ca-certificates dpkg-dev file \
    qt6-base-dev qt6-declarative-dev qt6-declarative-dev-tools \
    libkirigami-dev kirigami-addons-dev libkf6declarative-dev libkf6i18n-dev \
    libkf6dbusaddons-dev libkf6config-dev libkf6globalaccel-dev libkf6notifications-dev \
    libkf6statusnotifieritem-dev libkf6windowsystem-dev \
    liblayershellqtinterface-dev qtkeychain-qt6-dev zlib1g-dev \
    libxcb1-dev libxcb-keysyms1-dev libxcb-randr0-dev libxcb-xfixes0-dev libxcb-xtest0-dev \
    qml6-module-qtqml qml6-module-qtqml-models qml6-module-qtqml-workerscript \
    qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-dialogs \
    qml6-module-qtquick-layouts qml6-module-qtquick-templates qml6-module-qtquick-window \
    qml6-module-org-kde-kirigami qml6-module-org-kde-kirigamiaddons-components \
    qml6-module-org-kde-kirigamiaddons-delegates qml6-module-org-kde-kirigamiaddons-formcard \
    qml6-module-org-kde-kquickcontrols qml6-module-org-kde-desktop \
    desktop-file-utils appstream dbus-daemon xvfb xauth
