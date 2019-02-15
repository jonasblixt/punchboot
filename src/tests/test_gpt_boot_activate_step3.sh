#!/bin/bash
source tests/common.sh
wait_for_qemu_start
$PB boot -r
test_end_ok
