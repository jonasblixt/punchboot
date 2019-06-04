#!/bin/bash
touch /tmp/pb_force_recovery
source tests/common.sh
wait_for_qemu_start


echo Installing GPT
# Install GPT partitions
$PB part -i
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Done, listing partitions again"
$PB part -l
result_code=$?

# Test read GPT partition
# it should now work
if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Seting 'none' as active system"

$PB boot -a -s none

echo "Done"
test_end_ok
