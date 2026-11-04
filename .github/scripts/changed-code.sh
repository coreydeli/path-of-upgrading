#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Says, as a line for GITHUB_OUTPUT, whether a change touches anything the app
# is built from, as code=true or code=false. Docs and the issue forms don't
# change the build, so CI leaves it out for those. Main, beta and the packages
# are always built. A branch is compared with where it left beta, not with its
# last push, so a docs-only push on top of a code change is built too. Run it
# in a checkout with all of its history.
#
# PACKAGE is set when the packages are wanted, and BASE is the commit a pull
# request is into.
set -eu

# whether the build reads the file
is_code() {
    case $1 in
    src/* | tests/* | data/* | packaging/*) return 0 ;;
    *.md | docs/* | .github/ISSUE_TEMPLATE/* | .github/release.yml | .github/CODEOWNERS) return 1 ;;
    esac
    return 0
}

# whether none of the files on stdin are
only_docs() {
    while IFS= read -r file; do
        ! is_code "$file" || return 1
    done
}

# where the change starts from, if it can be found
start() {
    if [ -n "${BASE:-}" ]; then
        echo "$BASE"
    else
        git merge-base HEAD origin/beta 2> /dev/null
    fi
}

code=true
case ${GITHUB_REF:-} in
refs/heads/main | refs/heads/beta) ;;
*)
    # With renames off, the old path of a moved file counts too. Anything
    # that fails leaves it as code.
    if [ "${PACKAGE:-}" != true ] && base=$(start) \
        && files=$(git diff --name-only --no-renames "$base" HEAD) \
        && printf '%s\n' "$files" | only_docs; then
        code=false
    fi
    ;;
esac
echo "code=$code"
