#!/bin/bash
source tests/common.sh
wait_for_qemu_start
$PB boot -a -s none
$PB boot -r
wait_for_qemu
