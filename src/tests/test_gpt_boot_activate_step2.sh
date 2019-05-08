#!/bin/bash
source tests/common.sh
force_recovery_mode_on
wait_for_qemu_start
$PB boot -a -s none
force_recovery_mode_off
test_end_ok
