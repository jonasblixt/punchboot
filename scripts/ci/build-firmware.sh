#!/usr/bin/env bash
#
# Build the punchboot firmware binary for a board. Meant to run inside the
# punchboot-dev container (see scripts/pbdev), both locally and in CI.
#
#   scripts/ci/build-firmware.sh <board> [make args]
#
# Only the firmware binary (build-<board>/pb.bin) is built. Signed boot images
# need vendor tools (mkimage, NXP cst) and keys that are not in the
# container. Boards that use an external keystore are built against the
# internal test keystore.

set -euo pipefail

cd "$(dirname "$0")/../.."

board="${1:?Usage: $0 <board> [make args]}"
shift

case "$board" in
    jiffy)
        cross=arm-none-eabi-
        ;;
    imx8dxlevk)
        cross=aarch64-linux-gnu-
        # The defconfig points to a keystore that is not in the repository
        set -- CONFIG_KEYSTORE=pki/internal_keystore.bpak "$@"
        ;;
    *)
        echo "Unsupported board '$board'" >&2
        exit 1
        ;;
esac

make BOARD="src/board/$board" \
     KCONFIG_CONFIG="configs/${board}_defconfig" \
     CROSS_COMPILE="$cross" \
     -j"$(nproc)" \
     "$@" \
     "build-$board/pb.bin"
