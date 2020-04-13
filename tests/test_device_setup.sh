#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Execute device setup
$PB slc --set-configuration --force --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Get device data
$PB dev --show --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
