#!/bin/bash
source tests/common.sh
wait_for_qemu_start
V=-vvvvv

echo Dumping data

$PB part --dump /tmp/dump_data_test --part 2af755d8-8de5-45d5-a862-014cfa735ce0 \
            --transport socket $V

result_code=$?

if [ $result_code -ne 255 ];
then
    echo
    echo Dumping 'non dumpable' partition did not set error code
    echo
    test_end_error
fi

test_end_ok
