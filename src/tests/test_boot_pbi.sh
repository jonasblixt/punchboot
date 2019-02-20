#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=1023 count=64

$PBI tests/image1.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -a -s none

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
$PB boot -x -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
