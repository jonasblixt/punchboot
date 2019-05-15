#!/bin/bash
source tests/common.sh
wait_for_qemu_start

# Reset
$PB dev -w -y
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Authenticate
$PB dev -a -n 0 -f tests/test_auth_cookie -s RSA4096:sha256
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo "Trying to lock setup again..."
sync
sleep 0.1
$PB dev -w -y
result_code=$?

echo $result_code

if [ $result_code -ne 255 ];
then
    test_end_error
fi

test_end_ok
