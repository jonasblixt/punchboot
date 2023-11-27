#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_primary bs=512 count=1 skip=2082
dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_backup bs=512 count=1 skip=2083

$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup --write-board-reg 0 0x00000001
$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup --write-board-reg 1 0x00000000
$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup --write-board-reg 2 0x00000000
$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup --write-board-reg 3 0x00000000

dd if=/tmp/pb_config_primary of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2082 conv=notrunc
dd if=/tmp/pb_config_backup of=$CONFIG_QEMU_VIRTIO_DISK bs=512 count=1 seek=2083 conv=notrunc

# Reset
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
wait_for_qemu_start

dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_primary bs=512 count=1 skip=2082
dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_backup bs=512 count=1 skip=2083

echo "Sending reset"
$PB -t socket dev reset
wait_for_qemu

$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup --info
$PBSTATE -p /tmp/pb_config_primary -b /tmp/pb_config_backup --info | grep "3: 00000001"
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
