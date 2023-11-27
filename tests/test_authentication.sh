#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start

echo "Test auth"
# Listing partitions should work because security_state < 3
$PB -t socket part list


result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "set configuration"
# Lock device
$PB -t socket slc configure --force

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB -t socket slc show

echo "lock config"
$PB -t socket slc lock --force

result_code=$?

if [ $result_code -ne 0 ];
then
    echo "Failed to lock configuration, result_code = $result_code"
    test_end_error
fi

# Listing partitions should not work because security_state == 3
$PB -t socket part list
result_code=$?

if [ $result_code -ne 1 ];
then
    echo "part list failed $result_code"
    test_end_error
fi

$PB -t socket dev reset
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi

$PB -t socket boot partition $BOOT_A --verbose-boot
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi

$PB -t socket boot partition $BOOT_B --verbose-boot
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi

$PB -t socket boot enable $BOOT_A
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi


$PB -t socket boot enable $BOOT_B
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi

$PB -t socket boot bpak /tmp/random_data
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi

# Authenticate
$PB -t socket auth token tests/02e49231-756e-35ee-a982-378e5ba866a9.token  0xa90f9680
result_code=$?

if [ $result_code -ne 0 ];
then
    echo "Auth step failed $result_code"
    test_end_error
fi

sync
sleep 0.1

$PB -t socket part list
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

dd if=/dev/zero of=/tmp/disk bs=1M count=32 > /dev/null 2>&1
sync

test_end_ok
