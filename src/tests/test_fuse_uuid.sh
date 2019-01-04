#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Fuse random UUID
$PB fuse -w -n 2
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Get device data
$PB dev -l
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Fuse random UUID again, this should fail
$PB fuse -w -n 2
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

test_end_ok
