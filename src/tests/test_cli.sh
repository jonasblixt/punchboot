#!/bin/bash
touch /tmp/pb_force_recovery
source tests/common.sh
wait_for_qemu_start

# Display help banner
$PB
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi


$PB boot
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB dev
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB part
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
