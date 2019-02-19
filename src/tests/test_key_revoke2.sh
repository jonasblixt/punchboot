#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# This should fail since key 0 is now revoked

$PB boot -b -s a
result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi


test_end_ok
