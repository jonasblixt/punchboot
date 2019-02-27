#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Create pbimage
dd if=/dev/urandom of=/tmp/random_data bs=512k count=1

$PBI tests/image1.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Flashing image 
$PB part -w -n 0 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -a -s a

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi


test_end_ok
