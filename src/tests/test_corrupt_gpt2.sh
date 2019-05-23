#!/bin/bash
source tests/common.sh
wait_for_qemu_start


# Reset
$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# After a reboot, bootloader should have recovered from backup GPT tables
sync
echo Checking partitions
sfdisk -V /tmp/disk | grep "No errors detected"
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi


test_end_ok
