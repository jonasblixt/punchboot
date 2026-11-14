/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A data cache between the code under test and its device's model, see
 * dcache.h.
 */

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cmocka.h>

#include <pb/arch.h>
#include <pb/dma.h>

#include "dcache.h"

/*
 * A line the model knows. 'ram' is what memory holds, which the device
 * reads and writes. 'cpu' is what the cache held when the line was last
 * cleaned or invalidated: the process's memory differs from it when the CPU
 * wrote since, which is a dirty line.
 */
struct line {
    bool used;
    bool dev_wrote; /* The device wrote it, and it wasn't invalidated since */
    uintptr_t addr;
    uint8_t ram[DCACHE_LINE];
    uint8_t cpu[DCACHE_LINE];
};

/* A hash table on the line's address, which grows */
static struct line *lines;
static size_t n_slots;
static size_t n_lines;

bool dcache_faults_expected;
unsigned int dcache_faults;
const char *dcache_last_fault;

/* Addresses that are no memory here */
static struct {
    uintptr_t start;
    uintptr_t end;
} absent[4];

static size_t n_absent;

void dcache_no_memory(uintptr_t addr, size_t length)
{
    assert_true(n_absent < sizeof(absent) / sizeof(absent[0]));
    absent[n_absent].start = addr;
    absent[n_absent].end = addr + length;
    n_absent++;
}

static bool is_absent(uintptr_t addr)
{
    for (size_t i = 0; i < n_absent; i++) {
        if (addr >= absent[i].start && addr < absent[i].end)
            return true;
    }

    return false;
}

static void fault(const char *what, uintptr_t addr)
{
    dcache_faults++;
    dcache_last_fault = what;

    if (!dcache_faults_expected)
        fail_msg("The D-cache model, at 0x%zx: %s", (size_t)addr, what);
}

/*
 * The process's memory, a line at a time. Like a cache the model reads and
 * writes whole lines: around a buffer that isn't whole lines that is its
 * neighbours' memory and, with the address sanitizer, the red zones between
 * them, which these three may touch. Byte by byte through a volatile
 * pointer, so that no memcpy() the sanitizer intercepts is made of it.
 */
__attribute__((no_sanitize_address)) static void mem_get(uint8_t *dst, uintptr_t addr)
{
    const volatile uint8_t *p = (const volatile uint8_t *)addr;

    for (size_t i = 0; i < DCACHE_LINE; i++)
        dst[i] = p[i];
}

__attribute__((no_sanitize_address)) static void mem_put(uintptr_t addr, const uint8_t *src)
{
    volatile uint8_t *p = (volatile uint8_t *)addr;

    for (size_t i = 0; i < DCACHE_LINE; i++)
        p[i] = src[i];
}

__attribute__((no_sanitize_address)) static bool mem_differs(uintptr_t addr, const uint8_t *ref)
{
    const volatile uint8_t *p = (const volatile uint8_t *)addr;

    for (size_t i = 0; i < DCACHE_LINE; i++) {
        if (p[i] != ref[i])
            return true;
    }

    return false;
}

static struct line *slot(struct line *table, size_t slots, uintptr_t addr)
{
    for (size_t i = (addr / DCACHE_LINE) % slots;; i = (i + 1) % slots) {
        if (!table[i].used || table[i].addr == addr)
            return &table[i];
    }
}

static void grow(void)
{
    size_t slots = (n_slots == 0) ? 1024 : n_slots * 2;
    struct line *table = calloc(slots, sizeof(*table));

    if (table == NULL)
        abort();

    for (size_t i = 0; i < n_slots; i++) {
        if (lines[i].used)
            *slot(table, slots, lines[i].addr) = lines[i];
    }

    free(lines);
    lines = table;
    n_slots = slots;
}

/* The line that holds 'addr', NULL when the model doesn't know it and
 * 'create' is false */
static struct line *line_get(uintptr_t addr, bool create)
{
    struct line *l;

    addr &= ~(uintptr_t)(DCACHE_LINE - 1);

    if (n_slots != 0) {
        l = slot(lines, n_slots, addr);

        if (l->used)
            return l;
    }

    if (!create)
        return NULL;

    if (n_lines >= n_slots / 2)
        grow();

    /* Until now RAM and the cache agreed */
    l = slot(lines, n_slots, addr);
    n_lines++;
    l->used = true;
    l->dev_wrote = false;
    l->addr = addr;
    mem_get(l->ram, addr);
    mem_get(l->cpu, addr);
    return l;
}

static bool line_is_dirty(const struct line *l)
{
    return mem_differs(l->addr, l->cpu);
}

void dcache_reset(void)
{
    free(lines);
    lines = NULL;
    n_slots = 0;
    n_lines = 0;
    n_absent = 0;
    dcache_faults_expected = false;
    dcache_faults = 0;
    dcache_last_fault = NULL;
    dma_host_reset();
}

static void line_clean(struct line *l)
{
    if (!line_is_dirty(l))
        return;

    if (l->dev_wrote)
        fault("a clean writes a dirty line back over what the device wrote in it", l->addr);

    mem_get(l->ram, l->addr);
    mem_get(l->cpu, l->addr);
}

static void line_invalidate(struct line *l)
{
    if (line_is_dirty(l))
        fault("an invalidate discards what the CPU wrote in the line and didn't clean", l->addr);

    mem_put(l->addr, l->ram);
    memcpy(l->cpu, l->ram, DCACHE_LINE);
    l->dev_wrote = false;
}

static uintptr_t first_line(uintptr_t start)
{
    return start & ~(uintptr_t)(DCACHE_LINE - 1);
}

void arch_clean_cache_range(uintptr_t start, size_t len)
{
    for (uintptr_t a = first_line(start); a < start + len; a += DCACHE_LINE) {
        if (!is_absent(a))
            line_clean(line_get(a, true));
    }
}

void arch_invalidate_cache_range(uintptr_t start, size_t len)
{
    for (uintptr_t a = first_line(start); a < start + len; a += DCACHE_LINE) {
        if (!is_absent(a))
            line_invalidate(line_get(a, true));
    }
}

void arch_clean_invalidate_cache_range(uintptr_t start, size_t len)
{
    for (uintptr_t a = first_line(start); a < start + len; a += DCACHE_LINE) {
        struct line *l;

        if (is_absent(a))
            continue;

        l = line_get(a, true);
        line_clean(l);
        line_invalidate(l);
    }
}

void dcache_dev_read(uintptr_t addr, void *dst, size_t length)
{
    uint8_t *d = dst;

    /* A line at a time */
    while (length > 0) {
        size_t offset = addr % DCACHE_LINE;
        size_t chunk = DCACHE_LINE - offset;
        struct line *l;

        if (is_absent(addr))
            fail_msg("The device reads 0x%zx, which is no memory of the test's", (size_t)addr);

        l = line_get(addr, false);

        if (chunk > length)
            chunk = length;

        if (l == NULL) {
            fault("the device reads a line that was never cleaned", addr);
            l = line_get(addr, true);
        }

        memcpy(d, &l->ram[offset], chunk);
        d += chunk;
        addr += chunk;
        length -= chunk;
    }
}

void dcache_dev_write(uintptr_t addr, const void *src, size_t length)
{
    const uint8_t *s = src;

    /* A line at a time */
    while (length > 0) {
        size_t offset = addr % DCACHE_LINE;
        size_t chunk = DCACHE_LINE - offset;
        struct line *l;

        if (chunk > length)
            chunk = length;

        if (is_absent(addr))
            fail_msg("The device writes 0x%zx, which is no memory of the test's", (size_t)addr);

        if (!dma_host_in_begun((const void *)addr))
            fault("the device writes a line that no dma_in_begin() has begun", addr);

        l = line_get(addr, true);

        if (line_is_dirty(l))
            fault("the device writes a line the CPU has dirty", addr);

        memcpy(&l->ram[offset], s, chunk);
        l->dev_wrote = true;
        s += chunk;
        addr += chunk;
        length -= chunk;
    }
}

uint32_t dcache_dev_read_32(uintptr_t addr)
{
    uint32_t value;

    dcache_dev_read(addr, &value, sizeof(value));
    return value;
}

void dcache_dev_write_32(uintptr_t addr, uint32_t value)
{
    dcache_dev_write(addr, &value, sizeof(value));
}

bool dcache_is_dirty(const void *p, size_t length)
{
    uintptr_t start = (uintptr_t)p;

    for (uintptr_t a = first_line(start); a < start + length; a += DCACHE_LINE) {
        const struct line *l = line_get(a, false);

        if (l == NULL || line_is_dirty(l))
            return true;
    }

    return false;
}
