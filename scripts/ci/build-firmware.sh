#!/usr/bin/env bash
#
# Build the punchboot firmware binary for a board. Meant to run inside the
# punchboot-dev container (see scripts/pbdev), both locally and in CI.
#
#   scripts/ci/build-firmware.sh <board> [make args]
#
# Only the firmware binary (build-<board>/pb.bin) is built. Signed boot images
# are built by the 'all' target, which needs mkimage and NXP cst (both in the
# container) and, for most boards, vendor blobs that are not. Boards that use
# an external keystore are built against the internal test keystore.
#
# imx8mevk needs NXP's LPDDR4 training firmware, which is not in the
# repository. Without IMX8M_FW_DIR (a directory with the four
# lpddr4_pmu_train_*.bin files) it is built with zero filled placeholders:
# a compile check only, that pb.bin cannot train the DRAM.

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
    imx8mevk)
        cross=aarch64-linux-gnu-
        fw_dir="${IMX8M_FW_DIR:-}"
        if [ -z "$fw_dir" ]; then
            fw_dir="build-$board/placeholder-fw"
            echo "Using placeholder DDR training firmware: pb.bin is a compile check only" >&2
            mkdir -p "$fw_dir"
            for fw in 1d_imem 1d_dmem 2d_imem 2d_dmem; do
                head -c 1024 /dev/zero > "$fw_dir/lpddr4_pmu_train_$fw.bin"
            done
        fi
        set -- IMX8M_UMCTL_TRAIN_1D_I="$fw_dir/lpddr4_pmu_train_1d_imem.bin" \
               IMX8M_UMCTL_TRAIN_1D_D="$fw_dir/lpddr4_pmu_train_1d_dmem.bin" \
               IMX8M_UMCTL_TRAIN_2D_I="$fw_dir/lpddr4_pmu_train_2d_imem.bin" \
               IMX8M_UMCTL_TRAIN_2D_D="$fw_dir/lpddr4_pmu_train_2d_dmem.bin" \
               "$@"
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
