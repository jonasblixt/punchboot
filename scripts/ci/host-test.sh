#!/usr/bin/env bash
#
# Build punchboot for the host and run the unit and integration tests.
# Meant to run inside the punchboot-dev container (see scripts/pbdev),
# both locally and in CI.
#
# Arguments are passed on to the CMake configure step, e.g.
#   scripts/ci/host-test.sh -DPB_SANITIZE=ON
#
# Environment:
#   BUILD_DIR   Build directory (default: build-host)
#   CTEST_ARGS  Extra arguments for ctest, e.g. "-L unit"
#   PB_JOBS     Compile jobs at most, the firmware builds of the make tests
#               included (default: the CPUs this process may run on)

set -euo pipefail

cd "$(dirname "$0")/../.."

BUILD_DIR="${BUILD_DIR:-build-host}"
PB_JOBS="${PB_JOBS:-$(nproc)}"
export PB_JOBS
mkdir -p "$HOME"

echo "=== Installing punchboot host tools"
# setuptools builds in the source tree (build/, punchboot.egg-info) unless
# told otherwise: two runs at once in one tree collided there (F-400). Each
# BUILD_DIR has its own.
python_build="$(realpath -m "$BUILD_DIR")/python"
mkdir -p "$python_build"
printf '[build]\nbuild_base = %s\n[egg_info]\negg_base = %s\n' \
    "$python_build" "$python_build" > "$python_build/setup.cfg"
DIST_EXTRA_CONFIG="$python_build/setup.cfg" \
    pip install --quiet --no-build-isolation --no-deps .

echo "=== Building pbstate"
# With the run's arguments: -DPB_SANITIZE=ON builds pbstate with the
# sanitizers too. Those pbstate doesn't have are left unused
cmake -S tools/pbstate -B "$BUILD_DIR/pbstate" -DCMAKE_INSTALL_PREFIX=/opt/pb \
    --no-warn-unused-cli "$@" > /dev/null
cmake --build "$BUILD_DIR/pbstate" -j"$PB_JOBS" > /dev/null
cmake --install "$BUILD_DIR/pbstate" > /dev/null

echo "=== Building"
cmake -S . -B "$BUILD_DIR" "$@" > /dev/null
cmake --build "$BUILD_DIR" -j"$PB_JOBS"

echo "=== Running tests"
# shellcheck disable=SC2086
ctest --test-dir "$BUILD_DIR" --output-on-failure ${CTEST_ARGS:-}
