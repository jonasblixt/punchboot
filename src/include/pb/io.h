/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __PB_IO_H__
#define __PB_IO_H__

#include <pb.h>
#include <arch/arch.h>


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

static inline void pb_setbit32(uint32_t bit, __iomem addr)
{
    volatile uint32_t reg = pb_read32(addr);
    reg |= bit;
    pb_write32(reg, addr);
}


static inline void pb_clrbit32(uint32_t bit, __iomem addr)
{
    volatile uint32_t reg = pb_read32(addr);
    reg &= ~bit;
    pb_write32(reg, addr);
}
#endif

