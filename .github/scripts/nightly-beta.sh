#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Says whether tonight's beta is worth making: it succeeds when the checked out
# beta has code the last release or beta didn't, and the version isn't out yet.
# Changes to the docs alone don't count. Run it in a checkout of beta with all
# of its history and tags.
set -eu
cd "$(dirname "$0")/../.."

if ! sh .github/scripts/release-tag.sh beta | grep -qx 'release=true'; then
    echo "The version is out already, so there's no beta to make. Raise the version first."
    exit 1
fi

# the newest release or beta, which needn't be on beta, as a release's merge is on main
last=$(git for-each-ref --sort=-creatordate --count=1 --format='%(refname:short)' 'refs/tags/v*')
if [ -z "$last" ]; then
    echo "Nothing's released yet."
    exit 0
fi
if git diff --quiet "$last" HEAD; then
    echo "Nothing has changed since $last."
    exit 1
fi
if BASE=$last GITHUB_REF='' PACKAGE='' sh .github/scripts/changed-code.sh | grep -qx 'code=true'; then
    echo "There's new code since $last."
    exit 0
fi
echo "Only the docs have changed since $last."
exit 1
