#!/bin/bash
source tests/common.sh
wait_for_qemu_start
$PB -t socket boot disable
$PB -t socket dev reset
wait_for_qemu
