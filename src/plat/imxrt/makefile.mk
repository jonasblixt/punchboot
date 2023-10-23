#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#
ifdef CONFIG_PLAT_IMXRT

cflags-y += -I src/plat/imxrt/include
cflags-y += -mtune=cortex-m7

src-y  += src/plat/imxrt/plat.c
src-y += src/plat/imxrt/slc_helpers.c
src-y += src/plat/imxrt/rot_helpers.c

endif
