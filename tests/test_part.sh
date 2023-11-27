#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start


echo Installing GPT
# Install GPT partitions
$PB -t socket part install 1eacedf3-3790-48c7-8ed8-9188ff49672b
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Done, listing partitions again"
$PB -t socket part list
result_code=$?

# Test read GPT partition
# it should now work
if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Seting 'none' as active system"

$PB -t socket boot disable

echo "Done"
test_end_ok
