#!/bin/bash
force_recovery_mode_on
source tests/common.sh
wait_for_qemu_start
$PB boot -a -s none
force_recovery_mode_off
test_end_ok
