#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start


echo Installing GPT
# Install GPT partitions
$PB part --install --part 1eacedf3-3790-48c7-8ed8-9188ff49672b --transport socket
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Done, listing partitions again"
$PB part --list --transport socket
result_code=$?

# Test read GPT partition
# it should now work
if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Seting 'none' as active system"

$PB boot --activate none --transport socket

echo "Done"
test_end_ok
