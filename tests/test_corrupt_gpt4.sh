#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# After a reboot, bootloader should have recovered from backup GPT tables
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Trash GPT partition header

dd if=/dev/urandom of=/tmp/disk bs=512 skip=2 conv=notrunc count=3
sync
# Reset
$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
