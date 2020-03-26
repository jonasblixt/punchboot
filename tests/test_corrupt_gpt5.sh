#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# After a reboot, bootloader should have recovered from backup GPT tables
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo sgdisk is reporting errors
    test_end_error
fi

# Reset
$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
