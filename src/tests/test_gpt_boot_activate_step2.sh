#!/bin/bash
touch /tmp/pb_force_recovery
source tests/common.sh
wait_for_qemu_start
$PB boot -a -s none
force_recovery_mode_off
test_end_ok
