#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=64k count=1

set -e
BPAK=bpak
IMG=/tmp/img.bpak
PKG_UUID=8df597ff-2cf5-42ea-b2b6-47c348721b75
PKG_UNIQUE_ID=$(uuidgen -t)
V=-vvv

# secp521

$BPAK create $IMG -Y --hash-kind sha512 --signature-kind secp521r1 $V

$BPAK add $IMG --meta bpak-package --from-string $PKG_UUID --encoder uuid $V
$BPAK add $IMG --meta bpak-package-uid --from-string $PKG_UNIQUE_ID --encoder uuid $V


$BPAK add $IMG --meta pb-load-addr --from-string 0x49000000 --part-ref kernel \
                      --encoder integer $V

$BPAK add $IMG --part kernel \
               --from-file /tmp/random_data $V

$BPAK set $IMG --key-id pb-development3 \
               --keystore-id pb $V

$BPAK sign $IMG --key pki/secp521r1-key-pair.pem
set +e

$PB -t socket boot bpak /tmp/img.bpak
result_code=$?

if [ $result_code -ne 0 ];
then
    echo "secp521r1 failed: $result_code"
    test_end_error
fi


wait_for_qemu2
start_qemu

wait_for_qemu_start
# secp384

set -e
$BPAK create $IMG -Y --hash-kind sha384 --signature-kind secp384r1 $V

$BPAK add $IMG --meta bpak-package --from-string $PKG_UUID --encoder uuid $V
$BPAK add $IMG --meta bpak-package-uid --from-string $PKG_UNIQUE_ID --encoder uuid $V


$BPAK add $IMG --meta pb-load-addr --from-string 0x49000000 --part-ref kernel \
                      --encoder integer $V

$BPAK add $IMG --part kernel \
               --from-file /tmp/random_data $V

$BPAK set $IMG --key-id pb-development2 \
               --keystore-id pb $V

$BPAK sign $IMG --key pki/secp384r1-key-pair.pem

set +e
$PB -t socket boot bpak /tmp/img.bpak
result_code=$?

if [ $result_code -ne 0 ];
then
    echo "secp384r1 failed: $result_code"
    test_end_error
fi


wait_for_qemu2
test_end_ok
