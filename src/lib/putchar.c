/*
 * Copyright (c) 2013-2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <plat.h>
#include <stdio.h>

int putchar(int c)
{
    plat_uart_putc(NULL, (unsigned char)c);
	return c;
}
