#!/bin/bash
source tests/common.sh
wait_for_qemu_start

$PB part --install --transport socket
# Reset
$PB dev --reset --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# First make sure that the current configs are OK
echo Checking disk

dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_primary bs=512 count=1 skip=2082
dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_backup bs=512 count=1 skip=2083

primary_sha256=$(sha256sum /tmp/pb_config_primary | cut -d ' ' -f 1)
backup_sha256=$(sha256sum /tmp/pb_config_backup | cut -d ' ' -f 1)

if [ $primary_sha256 != $backup_sha256  ];
then
    echo "SHA comparison failed $primary_sha256 != $backup_sha256"
    test_end_error
fi

# Trash backup config
echo
echo Trashing backup config
echo
dd if=/dev/urandom of=$CONFIG_QEMU_VIRTIO_DISK conv=notrunc bs=512 count=1 seek=2083
echo
echo Restarting....
echo
wait_for_qemu
start_qemu
wait_for_qemu_start
echo
echo Checking config
echo

dd if=$CONFIG_QEMU_VIRTIO_DISK of=/tmp/pb_config_primary bs=512 count=1 skip=2082
primary_sha256=$(sha256sum /tmp/pb_config_primary | cut -d ' ' -f 1)

if [ $primary_sha256 != $backup_sha256  ];
then
    echo "SHA comparison failed $primary_sha256 != $backup_sha256"
    test_end_error
fi

test_end_ok
