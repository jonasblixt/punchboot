#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=1k count=16
set -e
BPAK=bpak
IMG=/tmp/img.bpak
PKG_UUID=8df597ff-2cf5-42ea-b2b6-47c348721b75
PKG_UNIQUE_ID=$(uuidgen -t)
V=-vvv

$BPAK create $IMG -Y --hash-kind sha256 --signature-kind rsa4096 $V

$BPAK add $IMG --meta bpak-package --from-string $PKG_UUID --encoder uuid $V
$BPAK add $IMG --meta bpak-package-uid --from-string $PKG_UNIQUE_ID --encoder uuid $V


$BPAK add $IMG --meta pb-load-addr --from-string 0x49000000 --part-ref kernel \
                      --encoder integer $V

$BPAK add $IMG --part kernel \
               --from-file /tmp/random_data $V

$BPAK sign $IMG --key pki/secp256r1-key-pair.pem \
                    --key-id pb-development \
                    --key-store pb $V
set +e
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Flashing A"
$PB part --write /tmp/img.bpak --part $BOOT_A --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Flashing B"
$PB part --write /tmp/img.bpak --part $BOOT_B --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

pbconfig -d /tmp/disk -v b
pbconfig -d /tmp/disk -s a -c 3

# Reset
force_recovery_mode_off
sync
$PB dev --reset --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
echo "Waiting for qemu to exit"
wait_for_qemu
echo "Starting qemu"
start_qemu
echo "QEMU started"
wait_for_qemu

boot_status=$(</tmp/pb_boot_status)
echo "Read boot status"

if [ "$boot_status" != "A" ];
then
    test_end_error
fi
echo 1/4
wait_for_qemu2
start_qemu
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

if [ "$boot_status" != "A" ];
then
    test_end_error
fi
echo 2/4
wait_for_qemu2
start_qemu
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

if [ "$boot_status" != "A" ];
then
    test_end_error
fi
echo 3/4
wait_for_qemu2
start_qemu
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

if [ "$boot_status" != "B" ];
then
    test_end_error
fi
echo 4/4
wait_for_qemu2
test_end_ok
