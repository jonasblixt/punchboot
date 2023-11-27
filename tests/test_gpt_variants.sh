#!/bin/bash
source tests/common.sh
wait_for_qemu_start

$PB -t socket part install 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 1
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Reset
echo "Reset"
$PB -t socket dev reset
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo Checking disk
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Disk error, rc=$result_code
    test_end_error
fi

wait_for_qemu
start_qemu
wait_for_qemu_start

echo "Installing table variant 2"

$PB -t socket part install 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 2
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Reset
echo "Reset"
$PB -t socket dev reset
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo Checking disk
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Disk error, rc=$result_code
    test_end_error
fi

wait_for_qemu
start_qemu
wait_for_qemu_start

echo "Installing table variant 3"

$PB -t socket part install 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 3
result_code=$?

if [ $result_code -ne 1 ];
then
    echo "Unexpected error code ($result_code)"
    test_end_error
fi

echo "Reset"
$PB -t socket dev reset
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Install default again

wait_for_qemu
start_qemu
wait_for_qemu_start

echo "Installing table variant 0"

$PB -t socket part install 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 0
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Reset
echo "Reset"
$PB -t socket dev reset
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
