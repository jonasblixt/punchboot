#!/bin/bash
source tests/common.sh
wait_for_qemu_start
sgdisk /tmp/disk -A 2:clear:63
wait_for_qemu
