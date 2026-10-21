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

set -euo pipefail

cd "$(dirname "$0")/../.."

BUILD_DIR="${BUILD_DIR:-build-host}"
mkdir -p "$HOME"

echo "=== Installing punchboot host tools"
pip install --quiet --no-build-isolation --no-deps .

echo "=== Building pbstate"
cmake -S tools/pbstate -B "$BUILD_DIR/pbstate" -DCMAKE_INSTALL_PREFIX=/opt/pb > /dev/null
cmake --build "$BUILD_DIR/pbstate" -j"$(nproc)" > /dev/null
cmake --install "$BUILD_DIR/pbstate" > /dev/null

echo "=== Building"
cmake -S . -B "$BUILD_DIR" "$@" > /dev/null
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo "=== Running tests"
# shellcheck disable=SC2086
ctest --test-dir "$BUILD_DIR" --output-on-failure ${CTEST_ARGS:-}
