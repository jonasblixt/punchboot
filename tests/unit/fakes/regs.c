/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A register file behind fakes/mmio (regs.h).
 */

#include "regs.h"

#include <pb/mmio.h>
#include <setjmp.h>
#include <stdarg.h>
#include <string.h>

#include <cmocka.h>

#define MAX_REGS   512
#define MAX_WRITES 4096

static struct {
    uintptr_t addr;
    uint64_t value;
    size_t reads;
} regs[MAX_REGS];

static size_t n_regs;

struct fake_reg_write fake_reg_log[MAX_WRITES];
size_t fake_reg_log_len;
uint64_t (*fake_regs_on_write)(uintptr_t addr, uint64_t value);
uint64_t (*fake_regs_on_read)(uintptr_t addr, uint64_t value);

static size_t reg_index(uintptr_t addr)
{
    for (size_t i = 0; i < n_regs; i++) {
        if (regs[i].addr == addr)
            return i;
    }

    assert_true(n_regs < MAX_REGS);
    regs[n_regs].addr = addr;
    regs[n_regs].value = 0;
    regs[n_regs].reads = 0;
    return n_regs++;
}

void fake_regs_reset(void)
{
    n_regs = 0;
    fake_reg_log_len = 0;
    fake_regs_on_write = NULL;
    fake_regs_on_read = NULL;
}

void fake_regs_set(uintptr_t addr, uint64_t value)
{
    regs[reg_index(addr)].value = value;
}

uint64_t fake_regs_get(uintptr_t addr)
{
    return regs[reg_index(addr)].value;
}

size_t fake_regs_writes(uintptr_t addr)
{
    return fake_regs_writes_in(addr, 1);
}

size_t fake_regs_reads(uintptr_t addr)
{
    return regs[reg_index(addr)].reads;
}

size_t fake_regs_first_write(uintptr_t addr)
{
    for (size_t i = 0; i < fake_reg_log_len; i++) {
        if (fake_reg_log[i].addr == addr)
            return i;
    }

    return fake_reg_log_len;
}

size_t fake_regs_writes_in(uintptr_t start, size_t size)
{
    size_t n = 0;

    for (size_t i = 0; i < fake_reg_log_len; i++) {
        if (fake_reg_log[i].addr >= start && fake_reg_log[i].addr - start < size)
            n++;
    }

    return n;
}

uint64_t fake_mmio_read(uintptr_t addr, unsigned int width)
{
    size_t i = reg_index(addr);

    (void)width;
    regs[i].reads++;

    if (fake_regs_on_read != NULL)
        regs[i].value = fake_regs_on_read(addr, regs[i].value);

    return regs[i].value;
}

void fake_mmio_write(uintptr_t addr, uint64_t value, unsigned int width)
{
    size_t i = reg_index(addr);

    assert_true(fake_reg_log_len < MAX_WRITES);
    fake_reg_log[fake_reg_log_len].addr = addr;
    fake_reg_log[fake_reg_log_len].value = value;
    fake_reg_log[fake_reg_log_len].width = width;
    fake_reg_log_len++;

    /* Stored first: the hook may not return */
    regs[i].value = value;

    if (fake_regs_on_write != NULL)
        regs[i].value = fake_regs_on_write(addr, value);
}
