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


$PB boot -a -s none

test_end_ok
