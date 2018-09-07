#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

PLAT_C_SRCS  += plat/imx6ul/imx_uart.c
PLAT_C_SRCS  += plat/imx6ul/ehci.c
PLAT_C_SRCS  += plat/imx6ul/usdhc.c
PLAT_C_SRCS  += plat/imx6ul/reset.c
PLAT_C_SRCS  += plat/imx6ul/gpt.c
PLAT_C_SRCS  += plat/imx6ul/caam.c
PLAT_C_SRCS  += plat/imx6ul/ocotp.c

plat_clean:
	@-rm -rf plat/imx6ul/*.o
