/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX8X_INCLUDE_PLAT_DEFS_H
#define PLAT_IMX8X_INCLUDE_PLAT_DEFS_H

#define COUNTER_FREQUENCY (8000000)
#define COUNTER_US_SHIFT (3)
#define CACHE_LINE 64

#define PLAT_VIRT_ADDR_SPACE_SIZE    (2ull << 32)
#define PLAT_PHY_ADDR_SPACE_SIZE    (2ull << 32)

#define MAX_XLAT_TABLES            32
#define MAX_MMAP_REGIONS        32

#define IMX_GPIO_BASE 0x5D080000

#define IMX_REG_BASE  0x50000000
#define IMX_REG_SIZE  0x10000000

#define PLATFORM_NS_UUID (const unsigned char *) "\xae\xda\x39\xbe\x79\x2b\x4d\xe5\x85\x8a\x4c\x35\x7b\x9b\x63\x02"

#endif  // PLAT_IMX8X_INCLUDE_PLAT_DEFS_H
