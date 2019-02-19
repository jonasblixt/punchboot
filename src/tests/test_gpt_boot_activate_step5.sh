#!/bin/bash
source tests/common.sh
wait_for_qemu_start
sync
sgdisk /tmp/disk -A 2:set:63
$PB boot -r
wait_for_qemu
