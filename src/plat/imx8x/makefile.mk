#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#

ifdef CONFIG_PLAT_IMX8X_COMMON

src-y  += src/plat/imx8x/plat.c
src-y  += src/plat/imx8x/fusebox.c
src-y  += src/plat/imx8x/slc_helpers.c
src-y  += src/plat/imx8x/rot_helpers.c
src-y  += src/plat/imx8x/sci/ipc.c
src-y  += src/plat/imx8x/sci/imx8_mu.c
src-y  += src/plat/imx8x/sci/svc/pad/pad_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/pm/pm_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/timer/timer_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/misc/misc_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/seco/seco_rpc_clnt.c
src-y  += src/plat/imx8x/sci/svc/rm/rm_rpc_clnt.c

asm-y += src/plat/imx8x/reset_vector.S

cflags-y += -mtune=cortex-a35
cflags-y += -DCACHE_LINE=64
cflags-y += -DPLAT_VIRT_ADDR_SPACE_SIZE=0x200000000
cflags-y += -DPLAT_PHY_ADDR_SPACE_SIZE=0x200000000
cflags-y += -DMAX_XLAT_TABLES=32
cflags-y += -DMAX_MMAP_REGIONS=32
cflags-y += -Iinclude/plat/imx8x/

ldflags-y += -Tsrc/plat/imx8x/link.lds

endif
