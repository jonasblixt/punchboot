#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Create pbimage
dd if=/dev/urandom of=/tmp/random_data bs=1024k count=1

# Set loading address so it overlaps with bootloader
set -e
BPAK=bpak
IMG=/tmp/img.bpak
PKG_UUID=8df597ff-2cf5-42ea-b2b6-47c348721b75
PKG_UNIQUE_ID=$(uuidgen -t)
V=-vvv

$BPAK create $IMG -Y --hash-kind sha256 --signature-kind rsa4096 $V

$BPAK add $IMG --meta bpak-package --from-string $PKG_UUID --encoder uuid $V
$BPAK add $IMG --meta bpak-package-uid --from-string $PKG_UNIQUE_ID --encoder uuid $V


$BPAK add $IMG --meta pb-load-addr --from-string 0x40011000 --part-ref kernel \
                      --encoder integer $V

$BPAK add $IMG --part kernel \
               --from-file /tmp/random_data $V

$BPAK set $IMG --key-id pb-development \
               --keystore-id pb $V

$BPAK sign $IMG --key pki/secp256r1-key-pair.pem
set +e
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Flashing image, should fail since image is larger then partition
echo Booting system A
$PB -t socket boot partition $BOOT_A
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi

test_end_ok
