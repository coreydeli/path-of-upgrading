#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Runs every test in the build folder it's started from, on a bus with no
# services and with an X server of their own for the tests that read one. The
# window tests are offscreen anyway.
set -eu
repo=$(cd "$(dirname "$0")/../.." && pwd)
# Qt wants one only you can use, and warns without it, which fails the tests
export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp/runtime-$(id -u)}"
mkdir -p -m 700 "$XDG_RUNTIME_DIR"
exec xvfb-run -a -s "-screen 0 1920x1080x24" \
    dbus-run-session --config-file="$repo/tests/bus.conf" -- \
    ctest --output-on-failure --timeout 300 -j "$(nproc)"
