/*
 * Copyright (c) 2012-2017 Roberto E. Vargas Caballero
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Portions copyright (c) 2018, ARM Limited and Contributors.
 * All rights reserved.
 */

#ifndef INCLUDE_PB_LIBC_STDLIB_H_
#define INCLUDE_PB_LIBC_STDLIB_H_

#include <stdlib_.h>

#ifndef NULL
#define NULL ((void *) 0)
#endif

#define _ATEXIT_MAX 1

extern void abort(void);
extern int atexit(void (*func)(void));
extern void exit(int status);

#endif  // INCLUDE_PB_LIBC_STDLIB_H_
