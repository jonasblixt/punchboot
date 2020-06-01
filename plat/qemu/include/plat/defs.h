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

#include <pb/pb.h>
#include <pb/io.h>

#define PB_BOOTPART_OFFSET 2

struct pb_platform_setup
{
    __iomem uart_base;
};

#define COUNTER_FREQUENCY (64000000)
#define COUNTER_US_SHIFT (6)

#endif  // PLAT_TEST_INCLUDE_PLAT_DEFS_H_
