#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=512k count=1

data_sha256=$(sha256sum /tmp/random_data | cut -d ' ' -f 1)

$PB boot -w -f /tmp/random_data
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi


# Extract data from disk image
dd if=/tmp/disk_aux of=/tmp/readback_boot0 bs=512 skip=12 count=1024
dd if=/tmp/disk_aux of=/tmp/readback_boot1 bs=512 skip=8204 count=1024

boot0_sha256=$(sha256sum /tmp/readback_boot0 | cut -d ' ' -f 1)
boot1_sha256=$(sha256sum /tmp/readback_boot1 | cut -d ' ' -f 1)


if [ $boot0_sha256 != $data_sha256  ];
then
    echo "SHA comparison for boot0 failed $boot0_sha256 != $data_sha256"
    test_end_error
fi

if [ $boot1_sha256 != $data_sha256  ];
then
    echo "SHA comparison for boot1 failed $boot1_sha256 != $data_sha256"
    test_end_error
fi


test_end_ok
