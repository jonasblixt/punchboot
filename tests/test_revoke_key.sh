#!/bin/bash
dd if=/dev/zero of=/tmp/disk bs=1M count=32 > /dev/null 2>&1
sync
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start
$PB -t socket part install 1eacedf3-3790-48c7-8ed8-9188ff49672b
$PB -t socket dev reset
echo "Waiting for reset"
wait_for_qemu

echo "Done"
start_qemu
wait_for_qemu_start

$PB -t socket slc show
$PB -t socket auth token tests/02e49231-756e-35ee-a982-378e5ba866a9.token 0xa90f9680
result_code=$?

if [ $result_code -ne 0 ];
then
    echo "Auth step failed $result_code"
    test_end_error
fi


echo "Test auth"
# Listing partitions should work because security_state < 3
$PB -t socket part list
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Revoke key
$PB -t socket slc revoke-key 0xa90f9680 --force
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB -t socket slc show

# Auth should now fail
$PB -t socket auth token tests/02e49231-756e-35ee-a982-378e5ba866a9.token 0xa90f9680
result_code=$?

if [ $result_code -ne 1 ];
then
    echo "Auth step2 failed $result_code"
    test_end_error
fi

test_end_ok
