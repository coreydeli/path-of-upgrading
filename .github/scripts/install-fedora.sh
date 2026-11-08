#!/bin/sh
# SPDX-License-Identifier: MIT
#
# What building, testing and packaging needs on Fedora, as root in the CI's
# container. The build requirements are the spec's own, so they can't drift
# from it, and they include Xvfb for the tests that read an X server. The
# Plasma style is what the window tests load when it's there.
set -eu
repo=$(cd "$(dirname "$0")/../.." && pwd)
dnf -y install --setopt=install_weak_deps=False \
    rpm-build rpmlint git ccache ca-certificates \
    kf6-qqc2-desktop-style qt6-qtwayland hicolor-icon-theme glibc-langpack-en
dnf -y builddep "$repo/packaging/rpm/path-of-upgrading.spec"
