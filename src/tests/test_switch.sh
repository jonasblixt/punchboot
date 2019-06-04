#!/bin/bash
touch /tmp/pb_force_recovery
source tests/common.sh
wait_for_qemu_start

dd if=/dev/urandom of=/tmp/random_data bs=1k count=16

$PBI tests/image1.pbc
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
echo Flashing sys A

$PB part -w -n 0 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

echo Flashing sys B
$PB part -w -n 1 -f /tmp/img.pbi
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi

# Activate System A
echo
echo Activating System A
echo

$PBCONFIG -d /tmp/disk -o 0x822 -b 0x823 -s a

# Reset
force_recovery_mode_off
sync
$PB boot -r
result_code=$?

if [ $result_code -ne 0 ];
then
    test_end_error
fi
echo "Waiting for qemu to exit"
wait_for_qemu
echo "Starting qemu"
start_qemu
echo "QEMU started"
wait_for_qemu

boot_status=$(</tmp/pb_boot_status)
echo "Read boot status"

echo 1/2
if [ "$boot_status" != "1" ];
then
    test_end_error
fi

# Activate System B

echo
echo Activating System B
echo

$PBCONFIG -d /tmp/disk -o 0x822 -b 0x823 -s b

wait_for_qemu2
start_qemu
wait_for_qemu
boot_status=$(</tmp/pb_boot_status)

echo 2/2

if [ "$boot_status" != "2" ];
then
    test_end_error
fi

test_end_ok
