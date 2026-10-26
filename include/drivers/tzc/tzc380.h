/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_TZC_TZC380_H
#define INCLUDE_DRIVERS_TZC_TZC380_H

#include <pb/utils_def.h>
#include <stdint.h>

/* Region security permissions (region_attributes_<n>.sp) */
#define TZC380_SP_NS_W BIT(28)
#define TZC380_SP_NS_R BIT(29)
#define TZC380_SP_S_W  BIT(30)
#define TZC380_SP_S_R  BIT(31)
#define TZC380_SP_ALL  (TZC380_SP_NS_W | TZC380_SP_NS_R | TZC380_SP_S_W | TZC380_SP_S_R)

/**
 * Initialize the TZC-380 driver
 *
 * @param[in] base Base address of the TZC-380
 *
 * @return PB_OK on success
 *        -PB_ERR_NOT_SUPPORTED if the TZC-380 has no region besides region 0
 */
int tzc380_init(uintptr_t base);

/**
 * Configure and enable a region
 *
 * Region 0 covers the whole address space and can't be configured here.
 *
 * @param[in] region Region number, 1 to the number of regions - 1
 * @param[in] region_base Base address, aligned to size
 * @param[in] size Size in bytes, a power of two, at least 32 KiB
 * @param[in] sp Security permissions, TZC380_SP_*
 *
 * @return PB_OK on success
 *        -PB_ERR_STATE if the driver isn't initialized
 *        -PB_ERR_PARAM on a bad region, base, size or permission
 */
int tzc380_configure_region(unsigned int region, uint64_t region_base, uint64_t size, uint32_t sp);

#endif // INCLUDE_DRIVERS_TZC_TZC380_H
