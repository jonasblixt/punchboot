/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The DMA and cache rule (doc/interfaces/drivers.md 4), on the arch's cache
 * functions: which of them a direction needs, on which lines, is here and
 * nowhere else.
 */

#include <pb/arch.h>
#include <pb/assert.h>
#include <pb/dma.h>
#include <pb/pb.h>

/*
 * What a driver got wrong, not what a device or a host did: checked on the
 * host, where a test fails on it (drivers.md 4.4).
 *
 * Not in the firmware yet, also not in a debug build: pb's own callers still
 * hand drivers a buffer that doesn't start on a line (the GPT's protective
 * MBR, the A/B state, an image part with such a load address), and such a
 * buffer gets the cache operations it got before, on the lines it touches.
 * The assertion comes to a debug build with the cores' refusal and the
 * boot's bounce (drivers.md 4.3, F-282), which leave no such caller.
 */
#ifdef CONFIG_ARCH_HOST
#define dma_assert(e) assert(e)
#else
#define dma_assert(e) ((void)0)
#endif

#ifdef CONFIG_ARCH_HOST
/*
 * The host has no cache. It keeps the ranges that are begun and not ended:
 * a device may be writing them, so nothing else is begun or cleaned in
 * their lines, and only what was begun is ended.
 */
#define DMA_HOST_IN_OPEN_MAX 32

static struct {
    uintptr_t start;
    size_t length; /* Whole lines; 0: the entry is free */
} in_open[DMA_HOST_IN_OPEN_MAX];

static bool host_in_overlaps(uintptr_t start, size_t length)
{
    uintptr_t first = start & ~((uintptr_t)PB_DMA_ALIGN - 1);
    uintptr_t end = start + length;

    for (size_t i = 0; i < DMA_HOST_IN_OPEN_MAX; i++) {
        if (in_open[i].length != 0 && first < in_open[i].start + in_open[i].length &&
            in_open[i].start < end)
            return true;
    }

    return false;
}

static bool host_in_begin(uintptr_t start, size_t length)
{
    if (host_in_overlaps(start, length))
        return false;

    for (size_t i = 0; i < DMA_HOST_IN_OPEN_MAX; i++) {
        if (in_open[i].length == 0) {
            in_open[i].start = start;
            in_open[i].length = length;
            return true;
        }
    }

    return false;
}

static bool host_in_end(uintptr_t start, size_t length)
{
    for (size_t i = 0; i < DMA_HOST_IN_OPEN_MAX; i++) {
        if (in_open[i].length == length && in_open[i].start == start) {
            in_open[i].length = 0;
            return true;
        }
    }

    return false;
}

size_t dma_host_in_open(void)
{
    size_t count = 0;

    for (size_t i = 0; i < DMA_HOST_IN_OPEN_MAX; i++) {
        if (in_open[i].length != 0)
            count++;
    }

    return count;
}

bool dma_host_in_begun(const void *addr)
{
    return host_in_overlaps((uintptr_t)addr, 1);
}

void dma_host_reset(void)
{
    for (size_t i = 0; i < DMA_HOST_IN_OPEN_MAX; i++)
        in_open[i].length = 0;
}
#else
#define host_in_overlaps(start, length) false
#define host_in_begin(start, length)    true
#define host_in_end(start, length)      true
#endif

void dma_out(const void *addr, size_t length)
{
    if (length == 0)
        return;

    /* Cleaning a line that a device is writing puts the CPU's stale bytes
     * over the device's, when the line is dirty */
    dma_assert(!host_in_overlaps((uintptr_t)addr, length));

    arch_clean_cache_range((uintptr_t)addr, length);
}

void dma_in_begin(void *addr, size_t length)
{
    if (length == 0)
        return;

    dma_assert(dma_owns_lines(addr, 0));
    dma_assert(host_in_begin((uintptr_t)addr, PB_DMA_LINES(length)));

    /* Clean, not invalidate: the device may read the range before it writes
     * it, and what it doesn't write stays the CPU's. A line that isn't dirty
     * is never written back over the device's bytes. The arch rounds the end
     * up to a line */
    arch_clean_cache_range((uintptr_t)addr, length);
}

void dma_in_end(void *addr, size_t length)
{
    if (length == 0)
        return;

    dma_assert(dma_owns_lines(addr, 0));
    dma_assert(host_in_end((uintptr_t)addr, PB_DMA_LINES(length)));

    arch_invalidate_cache_range((uintptr_t)addr, length);
}
