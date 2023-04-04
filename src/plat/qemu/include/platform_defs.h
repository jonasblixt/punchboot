/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_INCLUDE_PLAT_DEFS_H
#define PLAT_TEST_INCLUDE_PLAT_DEFS_H

#define CACHE_LINE 32

#define COUNTER_FREQUENCY (64000000)
#define COUNTER_US_SHIFT (6)

#define PLAT_VIRT_ADDR_SPACE_SIZE    (1ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE    (1ull << 32)

#define MAX_XLAT_TABLES            32
#define MAX_MMAP_REGIONS        32

#define PLATFORM_NS_UUID (const unsigned char *)  "\x3f\xaf\xc6\xd3\xc3\x42\x4e\xdf\xa5\xa6\x0e\xb1\x39\xa7\x83\xb5"

#endif  // PLAT_TEST_INCLUDE_PLAT_DEFS_H
