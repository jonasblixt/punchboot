#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_QEMU

PB_ARCH_NAME = armv7a

CFLAGS += -mtune=cortex-a15 -I plat/qemu/include

PLAT_C_SRCS  += plat/qemu/uart.c
PLAT_C_SRCS  += plat/qemu/reset.c
PLAT_C_SRCS  += plat/qemu/pl061.c
PLAT_C_SRCS  += plat/qemu/semihosting.c
PLAT_C_SRCS  += plat/qemu/wdog.c
PLAT_C_SRCS  += plat/qemu/plat.c
PLAT_C_SRCS  += plat/qemu/virtio.c
PLAT_C_SRCS  += plat/qemu/virtio_serial.c
PLAT_C_SRCS  += plat/qemu/virtio_block.c
PLAT_C_SRCS  += plat/qemu/fuse.c
PLAT_C_SRCS  += plat/qemu/root_hash.c
PLAT_C_SRCS  += plat/qemu/transport.c
PLAT_C_SRCS  += plat/qemu/crypto.c

PLAT_ASM_SRCS += plat/qemu/semihosting_call.S

ifdef CONFIG_QEMU_ENABLE_TEST_COVERAGE
PLAT_C_SRCS  += plat/qemu/gcov.c
CFLAGS += -g -fprofile-arcs -ftest-coverage
endif

LDFLAGS += -Tplat/qemu/link.lds

QEMU ?= qemu-system-arm
QEMU_AUDIO_DRV = "none"
QEMU_FLAGS  = -machine virt -cpu cortex-a15 -m $(CONFIG_QEMU_RAM_MB)
QEMU_FLAGS += -nographic -semihosting
# Virtio serial port
QEMU_FLAGS += -device virtio-serial-device
QEMU_FLAGS += -chardev socket,path=/tmp/pb.sock,server,nowait,id=pb_serial
QEMU_FLAGS += -device virtserialport,chardev=pb_serial
# Virtio Main disk
QEMU_FLAGS += -device virtio-blk-device,drive=disk
QEMU_FLAGS += -drive id=disk,file=$(CONFIG_QEMU_VIRTIO_DISK),cache=none,if=none,format=raw
# Disable default NIC
QEMU_FLAGS += -net none

qemu: $(BUILD_DIR)/$(TARGET)
	@echo $(QEMU_FLAGS)
	@$(QEMU) $(QEMU_FLAGS) $(QEMU_AUX_FLAGS) -kernel $(BUILD_DIR)/$(TARGET)

endif
