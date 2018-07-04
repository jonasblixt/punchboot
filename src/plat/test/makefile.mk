#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a-ve

CFLAGS += -mtune=cortex-a15

C_SRCS  += plat/test/uart.c
C_SRCS  += plat/test/usb_test.c
C_SRCS  += plat/test/emmc_test.c
C_SRCS  += plat/test/crypto.c
C_SRCS  += plat/test/reset.c

plat_clean:
	@-rm -rf plat/test/*.o
