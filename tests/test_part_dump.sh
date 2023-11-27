#!/bin/bash
source tests/common.sh
wait_for_qemu_start
V=-vvvvv

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

dd if=/dev/urandom of=/tmp/dump_data_in bs=1024k count=1

dump_in_sha256=$(sha256sum /tmp/dump_data_in | cut -d ' ' -f 1)

echo Writing data
$PB -t socket part write /tmp/dump_data_in ff4ddc6c-ad7a-47e8-8773-6729392dd1b5
result_code=$?
if [ $result_code -ne 0 ];
then
    echo
    echo Could not write to partition
    echo
    test_end_error
fi

echo Dumping data

$PB -t socket part read ff4ddc6c-ad7a-47e8-8773-6729392dd1b5 /tmp/dump_data_out
result_code=$?
if [ $result_code -ne 0 ];
then
    echo
    echo Could not read from partition
    echo
    test_end_error
fi
dump_out_sha256=$(sha256sum /tmp/dump_data_out | cut -d ' ' -f 1)

if [ $dump_in_sha256 != $dump_out_sha256  ];
then
    echo "SHA comparison failed $dump_in_sha256 != $dump_out_sha256"
    test_end_error
fi

test_end_ok
