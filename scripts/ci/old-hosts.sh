#!/usr/bin/env bash
#
# Released hosts against today's device (D-008, roadmap guard 3): installs
# punchboot releases from PyPI, each in a directory of its own that comes
# first on the container's Python path (its click and semver), pinned by the
# hash of its wheel, and runs
# tests/integration/test_old_hosts.py against pb-host with them.
#
#   scripts/pbdev run scripts/ci/old-hosts.sh
#
# The releases are those with a wheel for the container's Python 3.13: 2.3.0,
# 2.7.0 and 2.7.1 (2.0.0 to 2.2.0 have none). Needs the network for PyPI.
#
# Environment:
#   BUILD_DIR   pb-host's build directory (default: build-host), built here
#               when it has no pb-host

set -euo pipefail

BUILD_DIR="${BUILD_DIR:-build-host}"

# version sha256 of the manylinux x86_64 cp313 wheel on PyPI
RELEASES="
2.3.0 16295d0cc8e0e6e4688ae1e32bfcf8de8c8ef675f73abf4fad50a9ba52750453
2.7.0 2b259e543b4f9daaf63e5fb8277ae75b40de0293aaeca89389b2859bf50a86ca
2.7.1 6c240480929ab928c4306c4c17216810b8311bf8d7496c52a9d322244ec87eda
"

cd "$(dirname "$0")/../.."
mkdir -p "$HOME"

# Today's host tools, which the tests' harness uses, as host-test.sh installs them
python_build="$(realpath -m "$BUILD_DIR")/python"
mkdir -p "$python_build"
printf '[build]\nbuild_base = %s\n[egg_info]\negg_base = %s\n' \
    "$python_build" "$python_build" > "$python_build/setup.cfg"
DIST_EXTRA_CONFIG="$python_build/setup.cfg" \
    pip install --quiet --no-build-isolation --no-deps .

if [ ! -x "$BUILD_DIR/pb-host" ]; then
    cmake -S . -B "$BUILD_DIR" > /dev/null
    cmake --build "$BUILD_DIR" -j"${PB_JOBS:-$(nproc)}" --target pb-host pb-host-speculative > /dev/null
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

hosts=""
while read -r version digest; do
    [ -n "$version" ] || continue
    dir="$work/punchboot-$version"
    echo "punchboot==$version --hash=sha256:$digest" > "$work/req-$version.txt"
    pip install --quiet --no-deps --only-binary :all: --require-hashes \
        --target "$dir/lib" -r "$work/req-$version.txt"
    mkdir -p "$dir/bin"
    printf '#!/bin/sh\nPYTHONPATH="%s" exec python3 -m punchboot "$@"\n' "$dir/lib" > "$dir/bin/punchboot"
    chmod +x "$dir/bin/punchboot"
    hosts="${hosts:+$hosts:}$dir/bin/punchboot"
done <<< "$RELEASES"

PB_HOST_BIN="$BUILD_DIR/pb-host" \
    PB_HOST_SPECULATIVE_BIN="$BUILD_DIR/pb-host-speculative" \
    PB_OLD_HOSTS="$hosts" \
    python3 -m pytest tests/integration/test_old_hosts.py -q -p no:cacheprovider \
    --basetemp="$work/tmp"
