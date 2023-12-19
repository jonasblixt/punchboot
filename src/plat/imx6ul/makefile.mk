#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX6UL

cflags-y += -I src/plat/imx6ul/include
cflags-y += -mtune=cortex-a7
cflags-y += -DCACHE_LINE=32
cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32

ldflags-y += -Tsrc/plat/imx6ul/link.lds

src-y  += src/plat/imx6ul/plat.c
src-y  += src/plat/imx6ul/rot_helpers.c
src-y  += src/plat/imx6ul/slc_helpers.c
src-y  += src/plat/imx6ul/fusebox.c
src-y  += src/plat/imx6ul/hab.c

endif
