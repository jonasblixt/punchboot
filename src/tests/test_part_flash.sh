#!/bin/bash
source tests/common.sh
wait_for_qemu_start

sync
# Check that disk is OK
echo Checking disk
sgdisk -v /tmp/disk | grep "No problems found"
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi


# Create some dummy data of exactly the same size as partition

dd if=/dev/urandom of=/tmp/part_data_a bs=512k count=1
dd if=/dev/urandom of=/tmp/part_data_b bs=512k count=1

part_a_sha256=$(sha256sum /tmp/part_data_a | cut -d ' ' -f 1)
part_b_sha256=$(sha256sum /tmp/part_data_b | cut -d ' ' -f 1)

echo About to write data
# Flash data

$PB part -w -n 0 -f /tmp/part_data_a

result_code=$?

if [ $result_code -ne 0 ];
then
    echo
    echo Could not write to partition 0
    echo
    test_end_error
fi

$PB part -w -n 1 -f /tmp/part_data_b

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
echo Data written
sync

# Extract data from disk image
dd if=/tmp/disk of=/tmp/readback_a bs=512 skip=34 count=1024
dd if=/tmp/disk of=/tmp/readback_b bs=512 skip=1058 count=1024

readback_a_sha256=$(sha256sum /tmp/readback_a | cut -d ' ' -f 1)
readback_b_sha256=$(sha256sum /tmp/readback_b | cut -d ' ' -f 1)


if [ $part_a_sha256 != $readback_a_sha256  ];
then
    echo "SHA comparison for A part failed $readback_a_sha256 != $part_a_sha256"
    test_end_error
fi

if [ $part_b_sha256 != $readback_b_sha256  ];
then
    echo "SHA comparison for B part failed $readback_b_sha256 != $part_b_sha256"
    test_end_error
fi

test_end_ok
