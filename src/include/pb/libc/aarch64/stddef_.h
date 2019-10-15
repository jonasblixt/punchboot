/*
 * Copyright (c) 2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INCLUDE_PB_LIBC_AARCH64_STDDEF__H_
#define INCLUDE_PB_LIBC_AARCH64_STDDEF__H_

#ifndef SIZET_
typedef unsigned long size_t;
#define SIZET_
#endif

#ifndef _PTRDIFF_T
typedef long ptrdiff_t;
#define _PTRDIFF_T
#endif

#endif  // INCLUDE_PB_LIBC_AARCH64_STDDEF__H_
