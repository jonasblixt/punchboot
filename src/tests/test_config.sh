#!/bin/bash
source tests/common.sh
wait_for_qemu_start

$PB config -l
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB config -w -n 2 -v 1
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

#$PB config -w -n 123 -v 1
#result_code=$?

#if [ $result_code -ne 255 ];
#then
#    test_end_error
#fi

test_end_ok
