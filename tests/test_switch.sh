#!/bin/bash
touch /tmp/pb_force_command_mode
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

$BPAK set $IMG --key-id pb-development \
               --keystore-id pb $V

$BPAK sign $IMG --key pki/secp256r1-key-pair.pem
set +e
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
echo Flashing sys A

$PB part --write /tmp/img.bpak --part $BOOT_A --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo Flashing sys B
$PB part --write /tmp/img.bpak --part $BOOT_B --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Activate System A
echo
echo Activating System A
echo

# $PBSTATE -d /tmp/disk -s a

dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_primary bs=512 count=1 skip=2082
dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_backup bs=512 count=1 skip=2083

$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup -s a

dd if=/tmp/pb_config_primary of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2082 conv=notrunc
dd if=/tmp/pb_config_backup of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2083 conv=notrunc

#$PB boot --activate 2af755d8-8de5-45d5-a862-014cfa735ce0 --transport socket

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

echo 1/2
if [ "$boot_status" != "A" ];
then
    test_end_error
fi

# Activate System B

echo
echo Activating System B
echo

#$PBSTATE -d /tmp/disk -s b

dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_primary bs=512 count=1 skip=2082
dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_backup bs=512 count=1 skip=2083

$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup -s b

dd if=/tmp/pb_config_primary of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2082 conv=notrunc
dd if=/tmp/pb_config_backup of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2083 conv=notrunc
wait_for_qemu2
start_qemu
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

echo 2/2

if [ "$boot_status" != "B" ];
then
    test_end_error
fi

test_end_ok
