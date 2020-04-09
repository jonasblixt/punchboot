#!/bin/bash
source tests/common.sh
wait_for_qemu_start

sleep 0.1
# Listing partitions should work because security_state < 3
$PB part --list --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Lock device
$PB slc --set-configuration --force --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB slc --set-configuration-lock --force --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB dev --list --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Listing partitions should not work because security_state == 3
$PB part --list --transport socket
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB dev --reset --transport socket
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot --boot $BOOT_A --verbose-boot --transport socket
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot --boot $BOOT_B --verbose-boot --transport socket
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB part --activate $BOOT_A --transport socket
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi


$PB boot --activate $BOOT_B --transport socket
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

$PB boot --load /tmp/random_data --transport socket
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

# Authenticate
$PB auth --token tests2456a34c-d17a-3b6d-9157-bbe585b48e7b.token \
         --key-id 0xa90f9680 --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

sync
sleep 0.1

$PB part --list --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

test_end_ok
