#!/bin/bash
source tests/common.sh
wait_for_qemu_start
echo "Installing table"
$PB part --install --transport socket
# Reset
echo "Reset"
$PB dev --reset --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# First make sure that the current image is OK
echo Checking disk
sync
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Disk error
    test_end_error
fi

# Restart qemu
wait_for_qemu2
start_qemu
wait_for_qemu_start

# Print current layout
sgdisk -p /tmp/disk

# Shrink the partition by half
$PB part --force --resize 512 --part c046ccd8-0f2e-4036-984d-76c14dc73992 \
         --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Resize command failed
    test_end_error
fi

sgdisk -p /tmp/disk

sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Disk error
    test_end_error
fi

# Check resized partition details

sgdisk -i 2 /tmp/disk | grep "First sector: 1058"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo First sector is not 1058
    test_end_error
fi

sgdisk -i 2 /tmp/disk | grep "Last sector: 1569"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo Last sector is not 1569
    test_end_error
fi

# Check adjacent partition

sgdisk -i 3 /tmp/disk | grep "First sector: 1570"
result_code=$?

if [ $result_code -ne 0 ];
then
    echo First sector in adjacent partition is not 1570
    test_end_error
fi

test_end_ok
reset_disk
sync
