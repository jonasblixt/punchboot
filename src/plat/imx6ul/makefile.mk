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
ldflags-y += -Tsrc/plat/imx6ul/link.lds
src-y  += src/plat/imx6ul/plat.c


endif
