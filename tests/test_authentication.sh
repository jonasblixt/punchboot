#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start

echo "Test auth"
# Listing partitions should work because security_state < 3
$PB part --list --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "set configuration"
# Lock device
$PB slc --set-configuration --force --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "lock config"
$PB slc --set-configuration-lock --force --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    echo "Failed to lock configuration, result_code = $result_code"
    test_end_error
fi

# Listing partitions should not work because security_state == 3
$PB part --list --transport socket
result_code=$?

if [ $result_code -ne 253 ];
then
    echo "part list failed $result_code"
    test_end_error
fi

$PB dev --reset --transport socket
result_code=$?

if [ $result_code -ne 253 ];
then
    test_end_error
fi

$PB boot --boot $BOOT_A --verbose-boot --transport socket
result_code=$?

if [ $result_code -ne 253 ];
then
    test_end_error
fi

$PB boot --boot $BOOT_B --verbose-boot --transport socket
result_code=$?

if [ $result_code -ne 253 ];
then
    test_end_error
fi

$PB boot --activate $BOOT_A --transport socket
result_code=$?

if [ $result_code -ne 253 ];
then
    test_end_error
fi


$PB boot --activate $BOOT_B --transport socket
result_code=$?

if [ $result_code -ne 253 ];
then
    test_end_error
fi

$PB boot --load /tmp/random_data --transport socket
result_code=$?

if [ $result_code -ne 253 ];
then
    test_end_error
fi

# Authenticate
$PB auth --token tests/2456a34c-d17a-3b6d-9157-bbe585b48e7b.token \
         --key-id 0xa90f9680 --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    echo "Auth step failed $result_code"
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
