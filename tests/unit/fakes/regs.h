/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A register file behind fakes/mmio, for the tests of code with fixed
 * register addresses that needs no model of a device: a register reads what
 * was last written to it or set by the test, 0 before that, and every write
 * is logged in order.
 */

#ifndef TESTS_UNIT_FAKES_REGS_H
#define TESTS_UNIT_FAKES_REGS_H

#include <stddef.h>
#include <stdint.h>

struct fake_reg_write {
    uintptr_t addr;
    uint64_t value;
    unsigned int width;
};

/* Every write, in order */
extern struct fake_reg_write fake_reg_log[];
extern size_t fake_reg_log_len;

/* What the hardware does with a write, NULL for nothing: called after the
 * write is logged, with the value written, and returns the value the register
 * reads from then on (a bit that clears itself, a lock bit that sets). It may
 * leave with a longjmp: a write that resets the SoC */
extern uint64_t (*fake_regs_on_write)(uintptr_t addr, uint64_t value);

/* Every register reads 0, the log is empty, no fake_regs_on_write */
void fake_regs_reset(void);
/* What a register reads, without a write in the log */
void fake_regs_set(uintptr_t addr, uint64_t value);
uint64_t fake_regs_get(uintptr_t addr);
/* The number of writes to a register, and of reads of it */
size_t fake_regs_writes(uintptr_t addr);
size_t fake_regs_reads(uintptr_t addr);
/* The index in the log of the first write to a register, the log's length
 * when there is none */
size_t fake_regs_first_write(uintptr_t addr);
/* The number of writes to [start, start + size) */
size_t fake_regs_writes_in(uintptr_t start, size_t size);

#endif // TESTS_UNIT_FAKES_REGS_H
