/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __PB_IO_H_
#define __PB_IO_H_

#include <pb.h>

static inline uint32_t pb_readl(__iomem addr)
{
	return *((volatile uint32_t *)addr);
}

static inline void pb_writel(uint32_t data, __iomem addr)
{
	*((volatile uint32_t *)addr) = data;
}


static inline void pb_write32(uint32_t data, __iomem addr)
{
	*((volatile uint32_t *)addr) = data;
}

static inline void pb_write16(uint16_t data, __iomem addr)
{
	*((volatile uint16_t *)addr) = data;
}

static inline void pb_write8(uint8_t data, __iomem addr)
{
	*((volatile uint8_t *)addr) = data;
}

static inline uint32_t pb_read32(__iomem addr)
{
	return *((volatile uint32_t *)addr);
}

static inline uint16_t pb_read16(__iomem addr)
{
	return *((volatile uint16_t *)addr);
}

static inline uint8_t pb_read8(__iomem addr)
{
	return *((volatile uint8_t *)addr);
}

#endif

