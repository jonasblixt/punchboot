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
src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/plat_imx8mp.c
src-$(CONFIG_PLAT_IMX8MP) += src/plat/imx8m/ddr_imx8mp.c

# PB_RAM_LENGTH: the on-chip RAM pb runs from, starting at the board's
# PB_ENTRY (link.lds). IMX8M_OCRAM_SIZE: the OCRAM to map. IMX8M_ROM_SIZE: the
# boot ROM to map, for the ROM's APIs (HAB).
ifdef CONFIG_PLAT_IMX8MQ
PB_RAM_LENGTH = 0x38000
# What the i.MX8M Quad's BL31 takes isn't known, and no board of it has TF-A
PLAT_BL_PARAMS_LIMIT = 0xffffffff
cflags-y += -DIMX8M_OCRAM_SIZE=0x20000
cflags-y += -DIMX8M_ROM_SIZE=0x20000
endif
# i.MX8M Plus: the OCRAM free area after the ROM's part (RM, Figure 6-2), up to 0x97ffff
ifdef CONFIG_PLAT_IMX8MP
PB_RAM_LENGTH = 0x60000
PLAT_BL_PARAMS_LIMIT = 0x960000
cflags-y += -DIMX8M_OCRAM_SIZE=0x90000
# The boot ROM is 252 KiB, then 4 KiB protected (RM, memory map)
cflags-y += -DIMX8M_ROM_SIZE=0x3f000
endif
ldflags-y += --defsym=PB_RAM_LENGTH=$(PB_RAM_LENGTH)

# TF-A's bl_params (platform.md 5.3): BL31 takes them only from the OCRAM
# below PLAT_BL_PARAMS_LIMIT. plat.c has the memory, plat_bl_params, of
# PLAT_BL_PARAMS_SIZE bytes, and link.lds checks that it ends below the limit.
PLAT_BL_PARAMS_SIZE = 512
ifdef CONFIG_BOOT_ATF
cflags-y += -DPLAT_BL_PARAMS_SIZE=$(PLAT_BL_PARAMS_SIZE)
endif
ldflags-y += --defsym=PLAT_BL_PARAMS_SIZE=$(PLAT_BL_PARAMS_SIZE)
ldflags-y += --defsym=PLAT_BL_PARAMS_LIMIT=$(PLAT_BL_PARAMS_LIMIT)

cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x100000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32
ldflags-y += -Tsrc/plat/imx8m/link.lds

src-y  += src/drivers/pinctrl/imx_iomuxc.c
endif
