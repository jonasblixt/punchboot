#
# Punch BOOT
#
# Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#

ifdef CONFIG_PLAT_IMX8X

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

cflags-y += -I src/plat/imx8x/include
cflags-y += -mtune=cortex-a35
ldflags-y += -Tsrc/plat/imx8x/link.lds

endif
