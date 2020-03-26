#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# One block of random data for testing
dd if=/dev/urandom of=/tmp/random_data bs=512 count=1
first_sha256=$(sha256sum /tmp/random_data | cut -d ' ' -f 1)

$PB part -w -n 0 -o 1024 -f /tmp/random_data
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

dd if=/tmp/disk of=/tmp/readback_data bs=512 skip=1058 count=1

second_sha256=$(sha256sum /tmp/readback_data | cut -d ' ' -f 1)

if [ $first_sha256 != $second_sha256  ];
then
    echo "SHA comparison failed $first_sha256 != $second_sha256"
    test_end_error
fi

test_end_ok
