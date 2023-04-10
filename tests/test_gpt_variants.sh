#!/bin/bash
source tests/common.sh
wait_for_qemu_start

$PB part --install --part 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 1 --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Reset
echo "Reset"
$PB dev --reset --transport socket
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

$PB part --install --part 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 2 --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Reset
echo "Reset"
$PB dev --reset --transport socket
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

$PB part --install --part 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 3 --transport socket
result_code=$?

if [ $result_code -ne 240 ];
then
    echo "Unexpected error code ($result_code)"
    test_end_error
fi

echo "Reset"
$PB dev --reset --transport socket
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

$PB part --install --part 1eacedf3-3790-48c7-8ed8-9188ff49672b --variant 0 --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Reset
echo "Reset"
$PB dev --reset --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
