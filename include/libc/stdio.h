/*
 * Copyright (c) 2012-2017 Roberto E. Vargas Caballero
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Portions copyright (c) 2018, ARM Limited and Contributors.
 * All rights reserved.
 */

#ifndef INCLUDE_PB_LIBC_STDIO_H_
#define INCLUDE_PB_LIBC_STDIO_H_

#include <cdefs.h>
#include <stdarg.h>
#include <stdio_.h>

#ifndef NULL
#define NULL ((void *)(uintptr_t)0)
#endif

#define EOF -1

int printf(const char *fmt, ...) __printflike(1, 2);
int snprintf(char *s, size_t n, const char *fmt, ...) __printflike(3, 4);

int vprintf(const char *fmt, va_list args);

int putchar(int c);
int puts(const char *s);

#endif // INCLUDE_PB_LIBC_STDIO_H_
