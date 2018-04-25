#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a-ve

C_SRCS  += plat/imx6ul/imx_uart.c
C_SRCS  += plat/imx6ul/ehci.c
C_SRCS  += plat/imx6ul/usdhc.c
C_SRCS  += plat/imx6ul/reset.c
C_SRCS  += plat/imx6ul/gpt.c
C_SRCS  += plat/imx6ul/caam.c

plat_clean:
	@-rm -rf plat/imx6ul/*.o
