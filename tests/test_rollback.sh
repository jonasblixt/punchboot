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

$BPAK set $IMG --key-id pb-development \
               --keystore-id pb $V

$BPAK sign $IMG --key pki/secp256r1-key-pair.pem
set +e
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Flashing A"
$PB -t socket part write /tmp/img.bpak $BOOT_A
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Flashing B"
$PB -t socket part write /tmp/img.bpak $BOOT_B

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_primary bs=512 count=1 skip=2082
dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_backup bs=512 count=1 skip=2083

$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup -v b
$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup -s a -c 3

dd if=/tmp/pb_config_primary of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2082 conv=notrunc
dd if=/tmp/pb_config_backup of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2083 conv=notrunc

# Reset
force_recovery_mode_off
sync
$PB -t socket dev reset
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
