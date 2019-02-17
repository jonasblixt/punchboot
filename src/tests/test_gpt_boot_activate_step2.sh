#!/bin/bash
source tests/common.sh
wait_for_qemu_start
sgdisk /tmp/disk -A 1:set:63
wait_for_qemu
