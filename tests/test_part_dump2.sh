#!/bin/bash
source tests/common.sh
wait_for_qemu_start
V=-vvvvv

echo Dumping data

$PB -t socket part read 2af755d8-8de5-45d5-a862-014cfa735ce0 /tmp/dump_data_test
result_code=$?

if [ $result_code -ne 1 ];
then
    echo
    echo Unexpected result code $result_code
    echo
    test_end_error
fi

test_end_ok
