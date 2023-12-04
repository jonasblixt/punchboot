#!/bin/bash
touch /tmp/pb_force_command_mode
source tests/common.sh
wait_for_qemu_start
$PB -t socket boot disable
test_end_ok
