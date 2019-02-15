#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Create pbimage
dd if=/dev/urandom of=/tmp/random_data bs=512k count=1

# Set loading address so it overlaps with bootloader
$PBI -t LINUX -l 0x42000000 -f /tmp/random_data -k $KEY1 -n 0 -o /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Flashing image 
$PB part -w -n 1 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -a -n 2
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi


test_end_ok
