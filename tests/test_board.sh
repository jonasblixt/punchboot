#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start

$PB board --command test-command \
          --args 0x11223344 \
          --transport socket

result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Test unknown command
$PB board --command test-unknown \
          --args 0x00112233 \
          --transport socket

result_code=$?

if [ $result_code -ne 255 ];
then
    test_end_error
fi

# Test failing command 'test-command2'
$PB board --command test-command2 \
          --args 0x00112233 \
          --transport socket

result_code=$?

if [ $result_code -ne 255 ];
then
    echo "$$result_code = $result_code"
    test_end_error
fi

echo "Test ended OK"

test_end_ok
