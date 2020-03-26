#!/bin/bash
source tests/common.sh
wait_for_qemu_start

$PB part -i
# Reset
$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# First make sure that the current image is OK
echo Checking disk
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Disk error
    test_end_error
fi

# Trash primary GPT table
echo
echo Trashing primary GPT table
echo
dd if=/dev/urandom of=/tmp/disk conv=notrunc bs=512 count=64
sync

sgdisk -v /tmp/disk
result_code=$?

if [ $result_code -ne 2 ];
then
    echo
    echo "Disk reported OK but should have failed"
    echo
    test_end_error
fi


test_end_ok
sync
