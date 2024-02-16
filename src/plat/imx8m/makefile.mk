#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX8M

src-y  += src/plat/imx8m/plat.c
src-y  += src/plat/imx8m/umctl2_lp4.c

cflags-y += -DCACHE_LINE=64
cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32
ldflags-y += -Tsrc/plat/imx8m/link.lds

endif
