#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start

$PB -t socket board command test-command 0x11223344
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Test unknown command
$PB -t socket board command test-unknown 0x00112233
result_code=$?

if [ $result_code -ne 1 ];
then
    test_end_error
fi

# Test failing command 'test-command2'
$PB -t socket board command test-command2 0x00112233
result_code=$?

if [ $result_code -ne 1 ];
then
    echo "$$result_code = $result_code"
    test_end_error
fi

echo "Test ended OK"

test_end_ok
