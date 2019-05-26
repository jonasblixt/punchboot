#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=512k count=1

$PBI tests/image1.pbc

$PB part -w -n 1 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Create 1Mbyte PB Image, which will not fit in System A/B

dd if=/dev/urandom of=/tmp/random_data bs=1M count=1

$PBI tests/image1.pbc

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Flashing image should fail since it is to big

$PB part -w -n 0 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

# System B partition should still be intact

$PB boot -b -s b
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
test_end_ok
