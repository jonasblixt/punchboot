#
# Punch BOOT
#
# Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
#
# SPDX-License-Identifier: BSD-3-Clause
#
#


PB_ARCH_NAME = armv7a

CFLAGS += -mtune=cortex-a15

PLAT_C_SRCS  += plat/test/uart.c
PLAT_C_SRCS  += plat/test/usb_test.c
PLAT_C_SRCS  += plat/test/emmc_test.c
PLAT_C_SRCS  += plat/test/crypto.c
PLAT_C_SRCS  += plat/test/reset.c
PLAT_C_SRCS  += plat/test/pl061.c
PLAT_C_SRCS  += plat/test/semihosting.c
PLAT_C_SRCS  += plat/test/wdog.c
PLAT_C_SRCS  += plat/test/plat.c
PLAT_C_SRCS  += plat/test/gcov.c

PLAT_ASM_SRCS += plat/test/semihosting_call.S

CFLAGS += -fprofile-arcs -ftest-coverage

plat_clean:
	@-rm -rf plat/test/*.o plat/test/*.gcda plat/test/*.gcno
