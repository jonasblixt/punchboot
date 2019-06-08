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

# Trash backup GPT table
echo
echo Trashing backup GPT table
echo
dd if=/dev/urandom of=/tmp/disk conv=notrunc bs=512 count=34 seek=65502
sync

sgdisk -v /tmp/disk 2>0 | grep "CRC for the backup"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo
    echo "Disk reported OK but should have failed"
    echo
    test_end_error
fi

echo
echo Restarting....
echo
wait_for_qemu
start_qemu
wait_for_qemu_start
echo
echo Checking disk, pass 2
echo
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Disk error
    test_end_error
fi

test_end_ok
