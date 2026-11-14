/**
 * \file mmu.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The memory map (platform.md 5.2). pb_mmu_init() maps four layers, flat;
 * where regions nest, the innermost wins:
 *
 *  1. The board's DRAM, BOARD_DRAM_BASE and _SIZE, RW, execute never
 *  2. The platform's plat_mmap[]
 *  3. The board's regions (board.md 2.8)
 *  4. pb itself: .text RX, .rodata RO execute never, the rest of pb and the
 *     buffers RW execute never, the two as regions of their own
 */

#ifndef INCLUDE_PB_MMU_H
#define INCLUDE_PB_MMU_H

/* xlat_tables' region, which the platform's plat_mmap[] is a list of */
struct mmap_region;

/**
 * Build the translation tables from the four layers and turn the MMU on with
 * the arch's enable (platform.md 5.2). The platform calls it once, in
 * plat_init(), when DRAM works; nothing else of the platform's maps.
 *
 * A map with more regions than MAX_MMAP_REGIONS fails it with a log line.
 * Nothing is mapped then and the MMU stays off: the platform returns the
 * error from plat_init(), and main() resets.
 *
 * @param[in] plat_mmap The platform's regions, ended by an entry with a size
 *  and attributes of 0, or NULL for none
 *
 * @return PB_OK, or -PB_ERR_MEM for a map with too many regions
 */
int pb_mmu_init(const struct mmap_region *plat_mmap);

#endif // INCLUDE_PB_MMU_H
