#!/usr/bin/env bash
#
# Build the host tools and the QEMU test firmware, then run the integration
# tests. Meant to run inside the punchboot-dev container (see scripts/pbdev),
# both locally and in CI.
#
# Extra arguments are passed on to make, e.g. `scripts/ci/test.sh Q=`.

set -euo pipefail

cd "$(dirname "$0")/../.."

BUILD_DIR=build-test
mkdir -p "$HOME"

echo "=== Installing punchboot host tools"
pip install --quiet --no-build-isolation --no-deps .

echo "=== Building pbstate"
cmake -S tools/pbstate -B "$BUILD_DIR/pbstate" -DCMAKE_INSTALL_PREFIX=/opt/pb > /dev/null
cmake --build "$BUILD_DIR/pbstate" -j"$(nproc)" > /dev/null
cmake --install "$BUILD_DIR/pbstate" > /dev/null

echo "=== Building firmware and running tests"
cp configs/test_defconfig .config
make CROSS_COMPILE=arm-none-eabi- BOARD=src/board/test "$@" check
