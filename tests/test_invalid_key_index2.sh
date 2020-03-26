#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=64k count=1

# Sign with incorrect key index

$PBI tests/image_inv_key_index2.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -x -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi


$PBI tests/image_inv_key_index3.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -x -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi


$PBI tests/image_inv_key_index4.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -x -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PBI tests/image_inv_key_index5.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -x -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PBI tests/image_inv_key_index6.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB boot -x -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi
test_end_ok
