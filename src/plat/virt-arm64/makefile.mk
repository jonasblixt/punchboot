#
# Punch BOOT
#
# Copyright (C) 2024 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_VIRT_ARM64

PB_ARCH_NAME = armv8a

cflags-y += -mtune=cortex-a57 -I src/plat/virt-aarch64/include
cflags-y += -DCACHE_LINE=64
cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x200000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x200000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32

src-y  += src/plat/virt-arm64/plat.c
src-y  += src/plat/virt-arm64/uart.c
src-y  += src/plat/virt-arm64/wdog.c

ldflags-y += -Tsrc/plat/virt-arm64/link.lds

endif
