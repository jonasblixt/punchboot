#!/bin/bash
source tests/common.sh
wait_for_qemu_start


$PB part -l
result_code=$?

# Test read GPT partition
# this test should fail since the disk is empty
if [ $result_code -ne 255 ];
then
    test_end_error
fi

echo Installing GPT
# Install GPT partitions
$PB part -i
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

sleep 0.1

$PB part -l
result_code=$?

# Test read GPT partition
# it should now work
if [ $result_code -ne 0 ];
then
    test_end_error
fi

sgdisk /tmp/disk -A 1:set:63
sgdisk /tmp/disk -A 2:set:63

test_end_ok
