#!/usr/bin/env bash
#
# Creates the signed images of a test bench board:
#
#   root.bpak  the root file system for kickstart: rootfs.squashfs, its
#              dm-verity hash tree and the root hash in the signed header.
#              For the Root A/B partitions.
#   boot.bpak  the boot image for punchboot, for the System A/B
#              partitions:
#
#     atf      TF-A BL31 (aarch64 boards). punchboot starts it with the
#              kernel as BL33 (src/boot/atf.c)
#     kernel   Linux
#     dt       the device tree
#     ramdisk  kickstart.cpio.gz, the initramfs with kickstart as /init
#
# post-image.sh <images dir> <board>
#
# Both are signed with the punchboot test key, which punchboot's test
# keystore (pki/internal_keystore.bpak) and kickstart's test keystore trust.
# Set BOOT_SIGN_KEY and ROOT_SIGN_KEY to sign with other keys.

set -euo pipefail

BOARD="${2:?Usage: $0 <images dir> <board>}"
PB_DIR="$(realpath "$(dirname "$0")/../../..")"

BPAK="${HOST_DIR}/bin/bpak"
BOOT_SIGN_KEY="${BOOT_SIGN_KEY:-${PB_DIR}/pki/secp256r1-key-pair.pem}"
ROOT_SIGN_KEY="${ROOT_SIGN_KEY:-${PB_DIR}/pki/secp256r1-key-pair.pem}"
KEY_ID="pb-development"

# The GPT of both boards (src/board/<board>/board.c): System A/B are
# 30 MiB, Root A/B 128 MiB
BOOT_MAX_SIZE=$((30 * 1024 * 1024))
ROOT_MAX_SIZE=$((128 * 1024 * 1024))

die() {
    echo "post-image: $*" >&2
    exit 1
}

check_size() {
    local file="$1" max="$2" size

    size=$(stat -c %s "${file}")
    [ "${size}" -le "${max}" ] || die "${file} is ${size} bytes, the partition holds ${max}"
}

case "${BOARD}" in
jiffy)
    # punchboot uses 0x80001000-0x80027000 and 0x81021000-0x81847000
    # (src/plat/imx6ul/link.lds) and refuses to load over it. The zImage
    # decompresses itself to the start of the DRAM.
    ATF=
    KERNEL="${BINARIES_DIR}/zImage"
    KERNEL_ADDR=0x82000000
    DT="${BINARIES_DIR}/imx6ul-jiffy.dtb"
    DT_ADDR=0x83f00000
    RAMDISK_ADDR=0x84000000
    ;;
imx8mpevk)
    # BL31 runs from the OCRAM at 0x970000 (TF-A's BL31_BASE without
    # BL2), above punchboot (0x920000-0x966000). punchboot uses
    # 0x40000000-0x41060000 of the DRAM (src/plat/imx8m/link.lds). The
    # Image runs where it is loaded, 2 MiB aligned.
    ATF="${BINARIES_DIR}/bl31.bin"
    ATF_ADDR=0x970000
    KERNEL="${BINARIES_DIR}/Image"
    KERNEL_ADDR=0x42000000
    DT="${BINARIES_DIR}/imx8mp-evk-pb.dtb"
    DT_ADDR=0x50000000
    RAMDISK_ADDR=0x50100000
    ;;
imx8qxmek)
    # BL31 runs at 0x80000000, TF-A's fixed BL31_BASE, in the hole that
    # punchboot leaves for it (src/plat/imx8x/link.lds). punchboot uses
    # 0x80020000-0x80049000 and 0x85000000-0x85823000. The Image runs where
    # it is loaded, 2 MiB aligned (25.4 MiB with its bss); everything stays
    # below the DTS's first reserved memory, the VPU's at 0x84000000.
    ATF="${BINARIES_DIR}/bl31.bin"
    ATF_ADDR=0x80000000
    KERNEL="${BINARIES_DIR}/Image"
    KERNEL_ADDR=0x80200000
    DT="${BINARIES_DIR}/imx8qxp-mek-pb.dtb"
    DT_ADDR=0x83000000
    RAMDISK_ADDR=0x83100000
    ;;
*)
    die "unknown board ${BOARD}"
    ;;
esac

[ -f "${BOOT_SIGN_KEY}" ] || die "signing key ${BOOT_SIGN_KEY} not found"
[ -f "${ROOT_SIGN_KEY}" ] || die "signing key ${ROOT_SIGN_KEY} not found"

# Root image
ROOT_IMG="${BINARIES_DIR}/root.bpak"
rm -f "${ROOT_IMG}"
BPAK="${BPAK}" "${HOST_DIR}/bin/kickstart-mkroot" "${BINARIES_DIR}/rootfs.squashfs" \
    "${ROOT_SIGN_KEY}" kickstart "${KEY_ID}" "${ROOT_IMG}"
"${BPAK}" show "${ROOT_IMG}"
check_size "${ROOT_IMG}" "${ROOT_MAX_SIZE}"

# Boot image
BOOT_IMG="${BINARIES_DIR}/boot.bpak"
rm -f "${BOOT_IMG}"
"${BPAK}" create "${BOOT_IMG}" -Y --hash-kind sha256 --signature-kind prime256v1

add_part() {
    local name="$1" addr="$2" file="$3"

    "${BPAK}" add "${BOOT_IMG}" --meta pb-load-addr --from-string "${addr}" \
        --part-ref "${name}" --encoder integer
    "${BPAK}" add "${BOOT_IMG}" --part "${name}" --from-file "${file}"
}

if [ -n "${ATF}" ]; then
    add_part atf "${ATF_ADDR}" "${ATF}"
fi
add_part kernel "${KERNEL_ADDR}" "${KERNEL}"
add_part dt "${DT_ADDR}" "${DT}"
add_part ramdisk "${RAMDISK_ADDR}" "${BINARIES_DIR}/kickstart.cpio.gz"

"${BPAK}" set "${BOOT_IMG}" --key-id "${KEY_ID}" --keystore-id pb
"${BPAK}" sign "${BOOT_IMG}" --key "${BOOT_SIGN_KEY}"
"${BPAK}" show "${BOOT_IMG}"
check_size "${BOOT_IMG}" "${BOOT_MAX_SIZE}"
