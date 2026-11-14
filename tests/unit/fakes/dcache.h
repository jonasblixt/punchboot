/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A data cache between the code under test and a model of its device, for
 * the tests of drivers that do DMA (drivers.md 4). It is the arch's three
 * cache functions (pb/arch.h), so the driver reaches it through pb/dma.h as
 * it does on a board.
 *
 * The driver reads and writes the process's memory: that is the cache. The
 * device's model reads and writes RAM, with dcache_dev_read() and
 * dcache_dev_write(), never the memory itself. Where the two differ is kept
 * per line: a clean writes a dirty line to RAM, an invalidate drops the
 * cache's line for RAM's. A line is in the model from its first clean or
 * invalidate on; until then nothing was said about it, and RAM is taken to
 * hold what the cache does.
 *
 * So a driver that breaks the rule gets wrong bytes, as on a board: without
 * the clean the device reads old bytes, without the invalidate the driver
 * does. And what is silent on a board, or shows only now and then, is a
 * fault here, which fails the test:
 *
 *  - the device writes a line the CPU has dirty: the line's write back,
 *    whenever the cache chooses, puts the CPU's stale bytes over the
 *    device's (F-424: a job ring entry next to the driver's state)
 *  - a clean writes a dirty line back over what the device wrote in it
 *  - an invalidate discards what the CPU wrote in a line and didn't clean
 *    (F-081: a neighbour in the buffer's last line; F-134: a copy into the
 *    buffer before its invalidate)
 *  - the device writes a line that no dma_in_begin() has begun, or reads
 *    one that was never cleaned: memory the driver didn't hand to it
 */

#ifndef TESTS_UNIT_FAKES_DCACHE_H
#define TESTS_UNIT_FAKES_DCACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* The model's line, which is PB_DMA_ALIGN */
#define DCACHE_LINE 64

/* No line is in the model, no fault is counted, and no range is begun
 * (dma_host_reset()) */
void dcache_reset(void);

/* [addr, addr + length) is no memory of the test's: an address a driver is
 * given and only passes on. The model never touches it, and a device's
 * model that does fails the test. Until the next dcache_reset() */
void dcache_no_memory(uintptr_t addr, size_t length);

/* The device reads RAM, and writes it */
void dcache_dev_read(uintptr_t addr, void *dst, size_t length);
void dcache_dev_write(uintptr_t addr, const void *src, size_t length);

/* The device's view of a 32-bit word, and its write of one */
uint32_t dcache_dev_read_32(uintptr_t addr);
void dcache_dev_write_32(uintptr_t addr, uint32_t value);

/* Whether the CPU wrote in the range and didn't clean it. A line the model
 * doesn't know counts: nothing says it is in RAM */
bool dcache_is_dirty(const void *p, size_t length);

/* The faults so far, and the last one's text. A fault fails the test unless
 * dcache_faults_expected is set: for the tests of the model itself, and of
 * what a driver must not do */
extern bool dcache_faults_expected;
extern unsigned int dcache_faults;
extern const char *dcache_last_fault;

#endif // TESTS_UNIT_FAKES_DCACHE_H
