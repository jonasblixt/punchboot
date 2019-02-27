#!/bin/bash
source tests/common.sh
wait_for_qemu_start
$PB boot -s -s none
$PB boot -r
wait_for_qemu
