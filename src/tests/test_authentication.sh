#!/bin/bash
source tests/common.sh
wait_for_qemu_start

sleep 0.1
# Listing partitions should work because security_state < 3
$PB part -l

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Lock device
$PB dev -w -y

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB dev -l
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Listing partitions should not work because security_state == 3
$PB part -l
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot -r
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot -b -s A
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot -b -s B -v
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot -a -s A
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi


$PB boot -a -s B
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot -w -f /tmp/random_data
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi


$PB boot -x -f /tmp/random_data
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

# Authenticate
$PB dev -a -n 0 -f tests/test_auth_cookie -s RSA4096:sha256
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

sync
sleep 0.1

$PB part -l
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
