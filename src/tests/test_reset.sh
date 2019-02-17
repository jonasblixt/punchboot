#!/bin/bash
source tests/common.sh

wait_for_qemu_start
# Send reset command
$PB boot -r
wait_for_qemu


