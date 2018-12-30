#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Create 8Mbyte PB Image, which will not fit in System A/B

dd if=/dev/urandom of=/tmp/random_data bs=1M count=1

$PBI -t LINUX -l 0x49000000 -f /tmp/random_data -k $KEY1 -o /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB part -w -n 1 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot -s -b
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
test_end_ok
