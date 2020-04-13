#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Reset
$PB slc --set-configuration-lock --force --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Authenticate
$PB auth --token tests/2456a34c-d17a-3b6d-9157-bbe585b48e7b.token \
         --key-id 0xa90f9680 \
         --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Trying to lock setup again..."
sync
sleep 0.1
$PB slc --set-configuration-lock --force --transport socket
result_code=$?

echo $result_code

if [ $result_code -ne 255 ];
then
    test_end_error
fi

test_end_ok
