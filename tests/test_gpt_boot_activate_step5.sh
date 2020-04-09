#!/bin/bash
source tests/common.sh
wait_for_qemu_start
$PB boot --activate none --transport socket
$PB dev --reset --transport socket
wait_for_qemu
