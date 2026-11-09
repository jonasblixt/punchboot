#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMXRT

cflags-y += -mtune=cortex-m7

src-y  += src/plat/imxrt/plat.c
src-y += src/drivers/fuse/imx_hab_fuses.c

src-y  += src/drivers/pinctrl/imx_iomuxc.c
endif
