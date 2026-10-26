/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Arm TZC-380 TrustZone Address Space Controller (Arm DDI 0431)
 */

#include <drivers/tzc/tzc380.h>
#include <pb/mmio.h>
#include <pb/pb.h>

#define TZC380_CONFIGURATION              0x000
#define TZC380_CONFIGURATION_NO_REGION    GENMASK_32(3, 0)
#define TZC380_CONFIGURATION_ADDR_W       GENMASK_32(13, 8)
#define TZC380_CONFIGURATION_ADDR_W_SHIFT 8

#define TZC380_REGION_SETUP_LOW(n)        (0x100 + 0x10 * (n))
#define TZC380_REGION_SETUP_HIGH(n)       (0x104 + 0x10 * (n))
#define TZC380_REGION_ATTR(n)             (0x108 + 0x10 * (n))

#define TZC380_REGION_ATTR_SIZE_SHIFT     1 /* 2^(size + 1) bytes */
#define TZC380_REGION_ATTR_EN             BIT(0)

#define TZC380_REGION_MIN_SIZE_LOG2       15 /* 32 KiB */

static uintptr_t base;
static unsigned int no_of_regions;
static unsigned int addr_width;

int tzc380_init(uintptr_t base_)
{
    uint32_t config = mmio_read_32(base_ + TZC380_CONFIGURATION);

    if ((config & TZC380_CONFIGURATION_NO_REGION) == 0) {
        LOG_ERR("TZC-380 has no configurable region");
        return -PB_ERR_NOT_SUPPORTED;
    }

    base = base_;
    no_of_regions = (config & TZC380_CONFIGURATION_NO_REGION) + 1;
    addr_width = ((config & TZC380_CONFIGURATION_ADDR_W) >> TZC380_CONFIGURATION_ADDR_W_SHIFT) + 1;
    LOG_DBG("TZC-380: %u regions, %u bit addresses", no_of_regions, addr_width);
    return PB_OK;
}

int tzc380_configure_region(unsigned int region, uint64_t region_base, uint64_t size, uint32_t sp)
{
    unsigned int size_log2 = 0;

    if (base == 0)
        return -PB_ERR_STATE;

    if (region == 0 || region >= no_of_regions)
        return -PB_ERR_PARAM;

    if ((sp & ~TZC380_SP_ALL) != 0)
        return -PB_ERR_PARAM;

    if (size == 0 || (size & (size - 1)) != 0)
        return -PB_ERR_PARAM;

    while ((UINT64_C(1) << size_log2) != size)
        size_log2++;

    if (size_log2 < TZC380_REGION_MIN_SIZE_LOG2 || size_log2 > addr_width)
        return -PB_ERR_PARAM;

    if ((region_base & (size - 1)) != 0)
        return -PB_ERR_PARAM;

    /* With region_base aligned to size, the region fits if it starts inside the address space */
    if (addr_width < 64 && region_base >= (UINT64_C(1) << addr_width))
        return -PB_ERR_PARAM;

    mmio_write_32(base + TZC380_REGION_SETUP_LOW(region), (uint32_t)region_base);
    mmio_write_32(base + TZC380_REGION_SETUP_HIGH(region), (uint32_t)(region_base >> 32));
    mmio_write_32(base + TZC380_REGION_ATTR(region),
                  sp | ((size_log2 - 1) << TZC380_REGION_ATTR_SIZE_SHIFT) | TZC380_REGION_ATTR_EN);

    return PB_OK;
}
