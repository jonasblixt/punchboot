#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Execute device setup
$PB -t socket slc configure --force
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Get device data
$PB -t socket dev show
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
