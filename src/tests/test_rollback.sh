#!/bin/bash
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=1k count=16

$PBI tests/image1.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB part -w -n 0 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

$PB part -w -n 0 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Setup rollback GPT parameters

$PB dev -i -y -f tests/test_rollback.pbp
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Reset

$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

wait_for_qemu
start_qemu
wait_for_qemu_start
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

if [ "$boot_status" != "1" ];
then
    test_end_error
fi
echo 1/4
sync
sleep 0.1
start_qemu
wait_for_qemu_start
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

if [ "$boot_status" != "1" ];
then
    test_end_error
fi
echo 2/4
sync
sleep 0.1
start_qemu
wait_for_qemu_start
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

if [ "$boot_status" != "1" ];
then
    test_end_error
fi
echo 3/4
sync
sleep 0.1
start_qemu
wait_for_qemu_start
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

if [ "$boot_status" != "2" ];
then
    test_end_error
fi
echo 4/4
sync
sleep 0.1
test_end_ok
