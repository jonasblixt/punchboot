/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The DMA and cache rule, for every driver whose device reads or writes
 * memory itself (doc/interfaces/drivers.md 4). A driver includes this and no
 * arch header, and calls none of the arch's cache functions.
 *
 *  Out: the device reads memory the CPU wrote (data for a card, a
 *       descriptor, bytes to hash):
 *
 *           write it; dma_out(); start the device
 *
 *       and it isn't written again until the device is done with it.
 *
 *  In:  the device writes memory the CPU reads afterwards (a block, a USB
 *       transfer, a status), also when it reads it first (a descriptor it
 *       updates, a running context):
 *
 *           write what the device reads; dma_in_begin(); start the device;
 *           wait until it is done; dma_in_end(); read it
 *
 *       Between the two the CPU doesn't touch the range, and dma_in_end()
 *       comes before any copy into it. A driver that polls ends, reads, and
 *       begins again when the device isn't done.
 *
 * What a device writes owns its cache lines: it starts on a line and nothing
 * else is in its last line. A driver's own DMA memory is declared with
 * PB_DMA_BUFFER() or PB_DMA_ARRAY(); a caller's buffer is checked once, by
 * the class's core (dma_owns_lines()), before a driver sees it.
 */

#ifndef INCLUDE_PB_DMA_H
#define INCLUDE_PB_DMA_H

#include <pb/pb.h>

/**
 * The line size buffers are declared with: no smaller than the D-cache line
 * of any core pb runs on, which arch_init() checks. A constant, since an
 * alignment must be known when a buffer is declared; the cache operations
 * use the core's own line size for their stride.
 */
#define PB_DMA_ALIGN       64

/** 'size' rounded up to whole lines */
#define PB_DMA_LINES(size) (((size) + PB_DMA_ALIGN - 1) & ~((size_t)PB_DMA_ALIGN - 1))

/*
 * 'name' is the object itself, with its own type and size: an alias of
 * storage that starts on a line and is whole lines long, so that nothing
 * else is in its last line whatever the linker places after it. An alignment
 * on the object alone wouldn't pad it.
 */
#define PB_DMA_OBJECT_(decl, size, name, align)                   \
    PB_STATIC_ASSERT((align) % PB_DMA_ALIGN == 0, #name);         \
    static union {                                                \
        decl;                                                     \
        uint8_t lines[PB_DMA_LINES(size)];                        \
    } name##_lines __aligned(align) __section(".no_init") __used; \
    static decl __attribute__((alias(#name "_lines")))

/**
 * Declares 'static type name' as memory a device reads or writes: it starts
 * on a line and is padded to whole lines. In .no_init, so it is not
 * initialised, and it works after plat_init() (platform.md 5.1). At file
 * scope only.
 */
#define PB_DMA_BUFFER(type, name) PB_DMA_OBJECT_(type name, sizeof(type), name, PB_DMA_ALIGN)

/**
 * The same for 'static type name[n]'. The array owns its lines as a whole:
 * an element owns its own only when its type is whole lines long.
 */
#define PB_DMA_ARRAY(type, name, n) \
    PB_DMA_OBJECT_(type name[n], sizeof(type) * (n), name, PB_DMA_ALIGN)

/**
 * The same with an alignment of the device's own, a multiple of
 * PB_DMA_ALIGN: a page that a controller's pointers don't cross, a ring's
 * base.
 */
#define PB_DMA_ARRAY_ALIGNED(type, name, n, align) \
    PB_DMA_OBJECT_(type name[n], sizeof(type) * (n), name, align)

/**
 * Whether [addr, addr + length) starts on a line and is whole lines. With a
 * length of 0: whether 'addr' is on a line, for a buffer whose last line is
 * its own by its declaration.
 */
static inline bool dma_owns_lines(const void *addr, size_t length)
{
    return ((uintptr_t)addr % PB_DMA_ALIGN) == 0 && (length % PB_DMA_ALIGN) == 0;
}

/**
 * Out, before the device is started: what the CPU wrote in the range is in
 * memory. Any range: cleaning a whole line harms no neighbour.
 */
void dma_out(const void *addr, size_t length);

/**
 * In, before the device is started: no line of the range is dirty, and what
 * the CPU wrote there is in memory for the device to read. 'addr' is on a
 * line, which is asserted on the host (src/dma.c has why a debug build of
 * the firmware doesn't yet); the end is rounded up to one. That nothing else
 * is in the last line is the buffer's declaration.
 */
void dma_in_begin(void *addr, size_t length);

/**
 * In, after the device is done and before the CPU reads the range: the
 * cache's copy is dropped. The same range as dma_in_begin()'s. On the host,
 * a range that wasn't begun fails the assertion.
 */
void dma_in_end(void *addr, size_t length);

#ifdef CONFIG_ARCH_HOST
/*
 * The host has no cache, and checks what needs none (drivers.md 4.4): that a
 * range a device writes starts on a line, that every dma_in_begin() has its
 * dma_in_end() and that nothing is begun or cleaned inside a range that is
 * begun. For the tests:
 */

/** The number of ranges that are begun and not ended */
size_t dma_host_in_open(void);
/** Whether the line of 'addr' is in a range that is begun: a device may
 * write it */
bool dma_host_in_begun(const void *addr);
/** No range is begun */
void dma_host_reset(void);
#endif

#endif // INCLUDE_PB_DMA_H
