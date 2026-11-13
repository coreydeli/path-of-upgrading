#!/bin/sh
# SPDX-License-Identifier: MIT
#
# Says, as lines for GITHUB_OUTPUT, whether a change touches anything the app
# is built from (code=true or code=false), and whether it touches how the
# packages are made (packages=true or packages=false). Docs and the issue forms
# don't change the build, so the pipeline leaves it out for those. Run it in a
# checkout with all of its history.
#
# BASE is the commit the change is compared with: the branch a pull request is
# into, or what main had before a push. Without one, or when it isn't a commit
# here, everything counts as changed. PACKAGE is set when the packages are
# wanted anyway, for a release, which builds everything.
set -eu

# whether the build reads the file
is_code() {
    case $1 in
    src/* | tests/* | data/* | packaging/*) return 0 ;;
    *.md | docs/* | .gitignore | .github/ISSUE_TEMPLATE/* | .github/release.yml | .github/CODEOWNERS) return 1 ;;
    esac
    return 0
}

# whether the file is part of making the packages
is_packaging() {
    case $1 in
    .github/workflows/* | .github/scripts/* | packaging/*) return 0 ;;
    esac
    return 1
}

code=true
packages=false
if [ "${PACKAGE:-}" = true ]; then
    packages=true
# With renames off, the old path of a moved file counts too. Anything that
# fails counts as code.
elif [ -n "${BASE:-}" ] && git cat-file -e "$BASE^{commit}" 2> /dev/null \
    && files=$(git diff --name-only --no-renames "$BASE" HEAD); then
    code=false
    while IFS= read -r file; do
        [ -n "$file" ] || continue
        if is_code "$file"; then
            code=true
        fi
        if is_packaging "$file"; then
            packages=true
        fi
    done << EOF
$files
EOF
fi
echo "code=$code"
echo "packages=$packages"
