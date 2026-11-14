#!/usr/bin/env bash
#
# The compat CI job's guard (D-008, doc/system/compatibility.md "Guards"):
# against the commit a change starts from, abi.txt and the golden images'
# SHA256SUMS only grow, and no golden image or wire transcript is changed or
# removed.
#
#   scripts/ci/compat-guard.sh [base]
#
# Without a base, it is found from the CI's environment:
#   GITHUB_BASE_REF  A pull request's target branch: the merge base with it
#   BEFORE           A push's previous head (github.event.before)
#   neither          The merge base with origin/master
# A merge base that is HEAD itself, which a push to master whose previous
# head isn't in the clone gives, would compare the change with itself. The
# guard compares with HEAD's parent then, and says so.

set -euo pipefail

top="$(cd "$(dirname "$0")/../.." && pwd)"
null=0000000000000000000000000000000000000000

base="${1:-}"
if [ -z "$base" ]; then
    if [ -n "${GITHUB_BASE_REF:-}" ]; then
        base="$(git merge-base HEAD "origin/$GITHUB_BASE_REF")"
    elif [ -n "${BEFORE:-}" ] && [ "$BEFORE" != "$null" ] \
            && git cat-file -e "$BEFORE^{commit}" 2> /dev/null; then
        base="$BEFORE"
    else
        base="$(git merge-base HEAD origin/master)"
        if [ "$base" = "$(git rev-parse HEAD)" ]; then
            echo "No previous head to compare with, comparing with HEAD's parent"
            base="$(git rev-parse HEAD^)"
        fi
    fi
fi
echo "Comparing with $base"

python3 "$top/tests/compat/check_abi.py" history tests/compat/abi.txt --against "$base"
python3 "$top/tests/compat/check_abi.py" history tests/compat/bpak/SHA256SUMS --against "$base"
changed="$(git diff --name-only --diff-filter=DMRT "$base" -- \
    'tests/compat/bpak/*.bpak' 'tests/compat/transcripts/*.txt')"
if [ -n "$changed" ]; then
    echo "Golden bpak images and wire transcripts are never changed or removed (D-008):"
    echo "$changed"
    exit 1
fi
