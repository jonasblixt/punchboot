#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start

# Display help banner
$PB -t socket
result_code=$?

if [ $result_code -ne 2 ];
then
    test_end_error
fi


$PB -t socket boot
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB -t socket dev
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB -t socket part
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
