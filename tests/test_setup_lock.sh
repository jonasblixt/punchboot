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
$PB auth --token tests/02e49231-756e-35ee-a982-378e5ba866a9.token \
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
