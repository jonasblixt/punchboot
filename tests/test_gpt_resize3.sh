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

# Resize to anything below 1 should fail
$PB part --force --resize 0 --part c046ccd8-0f2e-4036-984d-76c14dc73992 \
         --transport socket
result_code=$?

if [ $result_code -ne 251 ];
then
    echo Resize command did not fail
    test_end_error
fi

test_end_ok
reset_disk
sync
