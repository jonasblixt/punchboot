#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Execute device setup
$PB dev -y -i -f tests/test.pbp
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

test_end_ok
