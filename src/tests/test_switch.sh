#!/bin/bash
touch /tmp/pb_force_recovery
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

$BPAK add $IMG --meta bpak-package --from_string $PKG_UUID --encoder uuid $V
$BPAK add $IMG --meta bpak-package-uid --from_string $PKG_UNIQUE_ID --encoder uuid $V


$BPAK add $IMG --meta pb-load-addr --from_string 0x49000000 --part-ref kernel \
                      --encoder integer $V

$BPAK add $IMG --part kernel \
               --from_file /tmp/random_data $V

$BPAK sign $IMG --key ../pki/dev_rsa_private.pem \
                    --key-id pb-development \
                    --key-store pb $V
set +e
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
echo Flashing sys A

$PB part -w -n 0 -f /tmp/img.bpak
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo Flashing sys B
$PB part -w -n 1 -f /tmp/img.bpak
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Activate System A
echo
echo Activating System A
echo

$PBCONFIG -d /tmp/disk -o 0x822 -b 0x823 -s a

# Reset
force_recovery_mode_off
sync
$PB boot -r
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

echo 1/2
if [ "$boot_status" != "1" ];
then
    test_end_error
fi

# Activate System B

echo
echo Activating System B
echo

$PBCONFIG -d /tmp/disk -o 0x822 -b 0x823 -s b

wait_for_qemu2
start_qemu
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

echo 2/2

if [ "$boot_status" != "2" ];
then
    test_end_error
fi

test_end_ok
