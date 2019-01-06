#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Check if it is possible to read configuration
$PB config -l
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Write "Force Recovery" Flag
$PB config -w -n 2 -v 1
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Write to an invalid key index
$PB config -w -n 123 -v 1
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

# Write to an out of bounds index
$PB config -w -n 255 -v 1
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

# Write to read only key
$PB config -w -n 0 -v 1
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

test_end_ok
