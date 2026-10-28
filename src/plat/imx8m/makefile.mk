#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMX8M_COMMON

src-y  += src/plat/imx8m/plat.c
src-y  += src/plat/imx8m/ddr.c
src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/plat_imx8mq.c
src-$(CONFIG_PLAT_IMX8MQ) += src/plat/imx8m/ddr_imx8mq.c

# PB_RAM_LENGTH: the on-chip RAM pb runs from, starting at the board's
# PB_ENTRY (link.lds). IMX8M_OCRAM_SIZE: the OCRAM to map.
ifdef CONFIG_PLAT_IMX8MQ
PB_RAM_LENGTH = 0x38000
cflags-y += -DIMX8M_OCRAM_SIZE=0x20000
endif
ldflags-y += --defsym=PB_RAM_LENGTH=$(PB_RAM_LENGTH)

cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32
ldflags-y += -Tsrc/plat/imx8m/link.lds

endif
