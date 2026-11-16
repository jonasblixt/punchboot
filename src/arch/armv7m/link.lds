/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


OUTPUT_FORMAT("elf32-littlearm", "elf32-littlearm", "elf32-littlearm")
OUTPUT_ARCH("arm")

/* The table pb writes to VTOR at its entry (entry.c) is aligned to a power
 * of two at least 4 times the number of exceptions the core has, and to 128
 * bytes at least (ARMv7-M ARM B3.2.5): 1024 covers a Cortex-M7 with all of
 * its 240 interrupts, the RT1060 has 160. PB_ENTRY gives it */
ASSERT((init_vector % 1024) == 0, "init_vector, pb's vector table, is not aligned to 1024 bytes")

/* The MPU region below the stack (entry.c, F-209) is 32 bytes, aligned to
 * its size, and ends where the stack starts */
ASSERT((_stack_start % 32) == 0, "_stack_start is not aligned to 32 bytes, the stack's guard region")
