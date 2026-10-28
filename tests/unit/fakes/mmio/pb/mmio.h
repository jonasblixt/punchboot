/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Replaces include/pb/mmio.h for unit tests of drivers that use fixed
 * register addresses. Every access goes to fake_mmio_read/write, which the
 * test implements (usually a model of the device).
 */

#ifndef TESTS_UNIT_FAKES_MMIO_PB_MMIO_H
#define TESTS_UNIT_FAKES_MMIO_PB_MMIO_H

#include <stdint.h>

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width);
void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width);

static inline void mmio_write_8(uintptr_t addr, uint8_t value)
{
    fake_mmio_write(addr, value, 8);
}

static inline uint8_t mmio_read_8(uintptr_t addr)
{
    return (uint8_t)fake_mmio_read(addr, 8);
}

static inline void mmio_write_16(uintptr_t addr, uint16_t value)
{
    fake_mmio_write(addr, value, 16);
}

static inline uint16_t mmio_read_16(uintptr_t addr)
{
    return (uint16_t)fake_mmio_read(addr, 16);
}

static inline void mmio_clrsetbits_16(uintptr_t addr, uint16_t clear, uint16_t set)
{
    mmio_write_16(addr, (mmio_read_16(addr) & ~clear) | set);
}

static inline void mmio_write_32(uintptr_t addr, uint32_t value)
{
    fake_mmio_write(addr, value, 32);
}

static inline uint32_t mmio_read_32(uintptr_t addr)
{
    return (uint32_t)fake_mmio_read(addr, 32);
}

static inline void mmio_write_64(uintptr_t addr, uint64_t value)
{
    fake_mmio_write(addr, value, 64);
}

static inline uint64_t mmio_read_64(uintptr_t addr)
{
    return fake_mmio_read(addr, 64);
}

static inline void mmio_clrbits_32(uintptr_t addr, uint32_t clear)
{
    mmio_write_32(addr, mmio_read_32(addr) & ~clear);
}

static inline void mmio_setbits_32(uintptr_t addr, uint32_t set)
{
    mmio_write_32(addr, mmio_read_32(addr) | set);
}

static inline void mmio_clrsetbits_32(uintptr_t addr, uint32_t clear, uint32_t set)
{
    mmio_write_32(addr, (mmio_read_32(addr) & ~clear) | set);
}

#endif
