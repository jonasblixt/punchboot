/*
 * Copyright 2020 Broadcom
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Portions copyright (c) 2020, ARM Limited and Contributors.
 * All rights reserved.
 */

#ifndef INTTYPES__H
#define INTTYPES__H

#define PRId64		"lld"	/* int64_t */
#define PRIi64		"lli"	/* int64_t */
#define PRIo64		"llo"	/* int64_t */
#define PRIu64		"llu"	/* uint64_t */
#define PRIx64		"llx"	/* uint64_t */
#define PRIX64		"llX"	/* uint64_t */

#define PRIdPTR         "ld"     /* intptr_t */
#define PRIiPTR         "li"     /* intptr_t */
#define PRIoPTR         "lo"     /* intptr_t */
#define PRIuPTR         "lu"     /* uintptr_t */
#define PRIxPTR         "lx"     /* uintptr_t */
#define PRIXPTR         "lX"     /* uintptr_t */

#endif /* INTTYPES__H */
