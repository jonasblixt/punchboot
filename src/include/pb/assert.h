/*
 * Copyright (c) 2018, ARM Limited and Contributors. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef ASSERT_H
#define ASSERT_H

#define assert(e)	((e) ? (void)0 : __assert(__FILE__, __LINE__, #e))

void __assert(const char *file, unsigned int line,
		      const char *assertion);

#endif /* ASSERT_H */
