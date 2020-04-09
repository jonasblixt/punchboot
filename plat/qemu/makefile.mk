#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

CFLAGS += -mtune=cortex-a15 -I plat/qemu/include

PLAT_C_SRCS  += plat/qemu/uart.c
PLAT_C_SRCS  += plat/qemu/reset.c
PLAT_C_SRCS  += plat/qemu/pl061.c
PLAT_C_SRCS  += plat/qemu/semihosting.c
PLAT_C_SRCS  += plat/qemu/wdog.c
PLAT_C_SRCS  += plat/qemu/plat.c
PLAT_C_SRCS  += plat/qemu/gcov.c
PLAT_C_SRCS  += plat/qemu/virtio.c
PLAT_C_SRCS  += plat/qemu/virtio_serial.c
PLAT_C_SRCS  += plat/qemu/virtio_block.c
PLAT_C_SRCS  += plat/qemu/test_fuse.c
PLAT_C_SRCS  += plat/qemu/root_hash.c
PLAT_C_SRCS  += plat/qemu/transport.c

PLAT_ASM_SRCS += plat/qemu/semihosting_call.S

CFLAGS += -fprofile-arcs -ftest-coverage

QEMU ?= qemu-system-arm
QEMU_AUDIO_DRV = "none"
QEMU_FLAGS  = -machine virt -cpu cortex-a15 -m 1024
QEMU_FLAGS += -nographic -semihosting
# Virtio serial port
QEMU_FLAGS += -device virtio-serial-device
QEMU_FLAGS += -chardev socket,path=/tmp/pb.sock,server,nowait,id=pb_serial
QEMU_FLAGS += -device virtserialport,chardev=pb_serial
# Virtio Main disk
QEMU_FLAGS += -device virtio-blk-device,drive=disk
QEMU_FLAGS += -drive id=disk,file=/tmp/disk,cache=none,if=none,format=raw
# Disable default NIC
QEMU_FLAGS += -net none