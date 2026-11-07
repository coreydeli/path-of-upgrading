#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Says what a push to a branch releases, as lines for GITHUB_OUTPUT: the
# version, the tag, whether to release, and the version the Arch Linux, Debian
# and Fedora packages say, and whether it's a beta, which has trade searches on
# by default. Main releases the version once, and beta releases
# v0.1.0-beta.1, v0.1.0-beta.2 and so on, until v0.1.0 is out. The packages of
# a beta come before the release's, as 0.1.0beta2 on Arch and 0.1.0~beta.2 on
# Debian and Fedora.
set -eu
cd "$(dirname "$0")/../.."
version=$(sh .github/scripts/check-version.sh)
tag=v$version
arch=$version
debian=$version
rpm=$version
release=false
beta=false
out=false
if git rev-parse -q --verify "refs/tags/$tag" > /dev/null; then
    out=true
fi
case ${1:-} in
main)
    if $out; then
        echo "::warning file=CMakeLists.txt::$tag is out already, so this releases nothing. Raise the version to release." >&2
    else
        release=true
    fi
    ;;
beta)
    if $out; then
        echo "::warning file=CMakeLists.txt::$tag is out already, so this releases nothing. Raise the version for the next beta." >&2
    else
        last=$(git tag -l "$tag-beta.*" | sed 's/.*-beta\.//' | sort -n | tail -n 1)
        n=$((${last:-0} + 1))
        tag=$tag-beta.$n
        arch=${version}beta$n
        debian=$version~beta.$n
        rpm=$version~beta.$n
        release=true
        beta=true
    fi
    ;;
esac
printf 'version=%s\ntag=%s\nrelease=%s\narch-version=%s\ndebian-version=%s\nrpm-version=%s\nbeta=%s\n' \
    "$version" "$tag" "$release" "$arch" "$debian" "$rpm" "$beta"
