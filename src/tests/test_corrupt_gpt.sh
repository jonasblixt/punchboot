#!/bin/bash
sync
source tests/common.sh
wait_for_qemu_start

# Reset
$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# First make sure that the current image is OK
sync
sgdisk -v /tmp/disk
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Trash primary GPT table

dd if=/dev/urandom of=/tmp/disk conv=notrunc bs=512 count=64
sync

sgdisk -v /tmp/disk
result_code=$?

if [ $result_code -ne 2 ];
then
    test_end_error
fi


test_end_ok
sync
