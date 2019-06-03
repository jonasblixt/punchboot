#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Create pbimage
dd if=/dev/urandom of=/tmp/random_data bs=256k count=1

# Set loading address so it overlaps with bootloader
$PBI tests/image4.pbc
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

# Loading image to ram and execute should fail because it overlaps with bootloader
echo Booting system A
$PB boot -b -s a
result_code=$?

if [ $result_code -ne 255 ];
then
    echo Boot step failed
    test_end_error
fi

echo Loading from RAM
# Loading image to ram and execute should fail because it overlaps with bootloader
$PB boot -x -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi


test_end_ok
