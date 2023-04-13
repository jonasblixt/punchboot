#!/bin/bash
dd if=/dev/zero of=/tmp/disk bs=1M count=32 > /dev/null 2>&1
sync
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start
$PB part --install --part 1eacedf3-3790-48c7-8ed8-9188ff49672b --transport socket
$PB dev --reset --transport socket
echo "Waiting for reset"
wait_for_qemu

echo "Done"
start_qemu
wait_for_qemu_start

$PB slc --show --transport socket
$PB auth --token tests/02e49231-756e-35ee-a982-378e5ba866a9.token  \
         --key-id 0xa90f9680 --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    echo "Auth step failed $result_code"
    test_end_error
fi


echo "Test auth"
# Listing partitions should work because security_state < 3
$PB part --list --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Revoke key
$PB slc --revoke-key 0xa90f9680 --force --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB slc --show --transport socket

# Auth should now fail
$PB auth --token tests/02e49231-756e-35ee-a982-378e5ba866a9.token \
         --key-id 0xa90f9680 --transport socket
result_code=$?

if [ $result_code -ne 242 ];
then
    echo "Auth step2 failed $result_code"
    test_end_error
fi

test_end_ok
