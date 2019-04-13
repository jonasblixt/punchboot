#!/bin/bash
sync
source tests/common.sh
wait_for_qemu_start


# First make sure that the current image is OK
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Trash primary GPT table

dd if=/dev/urandom of=/tmp/disk conv=notrunc bs=512 count=64
sync

# Make sure it is trashed
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 1 ];
then
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
sync
