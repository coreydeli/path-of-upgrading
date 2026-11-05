#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Prints the notes for a new release: the pull requests merged since the last
# one, as GitHub lists them, then every commit. A beta goes from the release or
# beta before it, and a release from the release before it.
#
# release-notes.sh <tag> <commit>
set -eu
cd "$(dirname "$0")/../.."
tag=$1
commit=$2
case $tag in
*-beta.*) since=$(git describe --tags --abbrev=0 "$commit" 2> /dev/null || true) ;;
*) since=$(git describe --tags --abbrev=0 --exclude='*-beta.*' "$commit" 2> /dev/null || true) ;;
esac
if [ -n "$since" ]; then
    gh api "repos/$GITHUB_REPOSITORY/releases/generate-notes" -f tag_name="$tag" \
        -f target_commitish="$commit" -f previous_tag_name="$since" --jq .body
    range=$since..$commit
else
    gh api "repos/$GITHUB_REPOSITORY/releases/generate-notes" -f tag_name="$tag" \
        -f target_commitish="$commit" --jq .body
    range=$commit
fi
echo
echo "## Every commit"
echo
git log --no-merges --format='- %s (%h)' "$range"
