#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# After a reboot, bootloader should have recovered from backup GPT tables
sync
sgdisk -v /tmp/disk
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Trash GPT header

dd if=/dev/urandom of=/tmp/disk bs=128 skip=2 conv=notrunc count=1

# Reset
$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
