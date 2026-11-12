#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Says, as a line for GITHUB_OUTPUT, whether a change touches anything the app
# is built from, as code=true or code=false. Docs and the issue forms don't
# change the build, so CI leaves it out for those. Beta and the packages are
# always built. A branch is compared with where it left beta, not with its last
# push, so a docs-only push on top of a code change is built too. A push to main
# that releases nothing is compared with what main had before it. Run it in a
# checkout with all of its history.
#
# PACKAGE is set when the packages are wanted, BASE is the commit a pull request
# is into, and BEFORE is what a pushed branch pointed at before the push.
set -eu

# whether the build reads the file
is_code() {
    case $1 in
    src/* | tests/* | data/* | packaging/*) return 0 ;;
    *.md | docs/* | .gitignore | .github/ISSUE_TEMPLATE/* | .github/release.yml | .github/CODEOWNERS) return 1 ;;
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

# whether everything from the commit given to HEAD is docs. With renames off,
# the old path of a moved file counts too. Anything that fails counts as code.
docs_since() {
    files=$(git diff --name-only --no-renames "$1" HEAD) || return 1
    printf '%s\n' "$files" | only_docs
}

code=true
case ${GITHUB_REF:-} in
refs/heads/beta) ;;
refs/heads/main)
    # a new branch's push has no commit before it
    if [ "${PACKAGE:-}" != true ] && [ -n "${BEFORE:-}" ] \
        && git cat-file -e "$BEFORE^{commit}" 2> /dev/null && docs_since "$BEFORE"; then
        code=false
    fi
    ;;
*)
    if [ "${PACKAGE:-}" != true ] && base=$(start) && docs_since "$base"; then
        code=false
    fi
    ;;
esac
echo "code=$code"
