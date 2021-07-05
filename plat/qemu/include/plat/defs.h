/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_INCLUDE_PLAT_DEFS_H_
#define PLAT_TEST_INCLUDE_PLAT_DEFS_H_

#define PB_BOOTPART_OFFSET 2


#define COUNTER_FREQUENCY (64000000)
#define COUNTER_US_SHIFT (6)


#define PLAT_VIRT_ADDR_SPACE_SIZE	(1ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE	(1ull << 32)

#define MAX_XLAT_TABLES			32
#define MAX_MMAP_REGIONS		32

#endif  // PLAT_TEST_INCLUDE_PLAT_DEFS_H_
