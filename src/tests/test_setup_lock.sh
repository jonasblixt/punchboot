#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Reset
$PB dev -w -y
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB dev -w -y
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

test_end_ok
