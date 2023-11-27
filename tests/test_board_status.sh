#!/bin/bash
source tests/common.sh
wait_for_qemu_start

$PB -t socket board status
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
