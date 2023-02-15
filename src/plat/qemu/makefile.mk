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

cflags-y += -mtune=cortex-a15 -I src/plat/qemu/include

src-y  += src/plat/qemu/uart.c
src-y  += src/plat/qemu/reset.c
src-y  += src/plat/qemu/pl061.c
src-y  += src/plat/qemu/semihosting.c
src-y  += src/plat/qemu/wdog.c
src-y  += src/plat/qemu/plat.c
src-y  += src/plat/qemu/virtio.c
src-y  += src/plat/qemu/virtio_serial.c
src-y  += src/plat/qemu/virtio_block.c
src-y  += src/plat/qemu/fuse.c
src-y  += src/plat/qemu/root_hash.c
src-y  += src/plat/qemu/transport.c
src-y  += src/plat/qemu/crypto.c

asm-y += src/plat/qemu/semihosting_call.S

src-$(CONFIG_QEMU_ENABLE_TEST_COVERAGE)  += src/plat/qemu/gcov.c
cflags-$(CONFIG_QEMU_ENABLE_TEST_COVERAGE) += -g -fprofile-arcs -ftest-coverage

ldflags-y += -Tsrc/plat/qemu/link.lds

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
