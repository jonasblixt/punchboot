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
 *  3. The board's regions, 'mmap' in its data (board.md 2.8)
 *  4. pb itself: .text RX, .rodata RO execute never, the rest of pb and the
 *     buffers RW execute never, the two as regions of their own
 *
 * A board lists what it reaches and the platform didn't map with the macros
 * below, and includes nothing of xlat_tables:
 *
 *   static const struct pb_mmap_region board_mmap[] = {
 *       PB_MAP_DEVICE(0x08000000, 0x00010000),    // The FPGA on chip select 0
 *       PB_MAP_MEMORY_RO(0x60000000, 0x01000000), // The NOR flash's window
 *       PB_MAP_END,
 *   };
 *
 * None is executable. Each is flat and whole pages, which the macros check
 * in the build, and outside pb, its buffers, the board's DRAM, the
 * platform's regions and the board's other regions, which pb_mmu_init()
 * checks. On an arch without an MMU (ARMv7-M) the list is accepted and not
 * read.
 */

#ifndef INCLUDE_PB_MMU_H
#define INCLUDE_PB_MMU_H

#include <stddef.h>
#include <stdint.h>

/* The translation granule: a region's base and size are multiples of it */
#define PB_MAP_PAGE_SIZE 4096

enum pb_map_kind {
    PB_MAP_KIND_END = 0, /*!< Ends the list */
    PB_MAP_KIND_DEVICE, /*!< Device memory, read and write */
    PB_MAP_KIND_MEMORY_RO, /*!< Normal memory, read only */
    PB_MAP_KIND_MEMORY_RW, /*!< Normal memory, read and write */
};

struct pb_mmap_region {
    uintptr_t base;
    size_t size;
    enum pb_map_kind kind;
};

/* 0 when base and size are whole pages and the size isn't 0. With constants
 * the build stops otherwise, on an array of negative size */
#define PB_MAP_CHECK_(base_, size_) \
    (0 *                            \
     sizeof(char[((((base_) | (size_)) & (PB_MAP_PAGE_SIZE - 1)) == 0 && (size_) != 0) ? 1 : -1]))

#define PB_MAP_REGION_(base_, size_, kind_) \
    { .base = (base_) + PB_MAP_CHECK_(base_, size_), .size = (size_), .kind = (kind_) }

/* Device memory, read and write */
#define PB_MAP_DEVICE(base_, size_)    PB_MAP_REGION_(base_, size_, PB_MAP_KIND_DEVICE)
/* Normal memory, read only: a flash that is booted in place (board.md 2.4) */
#define PB_MAP_MEMORY_RO(base_, size_) PB_MAP_REGION_(base_, size_, PB_MAP_KIND_MEMORY_RO)
/* Normal memory, read and write: RAM on an external bus */
#define PB_MAP_MEMORY_RW(base_, size_) PB_MAP_REGION_(base_, size_, PB_MAP_KIND_MEMORY_RW)
/* Ends the list */
#define PB_MAP_END                     { .base = 0, .size = 0, .kind = PB_MAP_KIND_END }

/* xlat_tables' region, which the platform's plat_mmap[] is a list of */
struct mmap_region;

/**
 * Build the translation tables from the four layers and turn the MMU on with
 * the arch's enable (platform.md 5.2). The platform calls it once, in
 * plat_init(), when DRAM works; nothing else of the platform's maps.
 *
 * The board's regions are checked first. One that is of no kind, isn't
 * whole pages, wraps or is outside the address space, or that overlaps pb,
 * its buffers, the board's DRAM, a region of plat_mmap[] or another of the
 * board's, fails it with a log line that names it; so does a map with more
 * regions than MAX_MMAP_REGIONS. Nothing is mapped then and the MMU stays
 * off: the platform returns the error from plat_init(), and main() resets.
 *
 * @param[in] plat_mmap The platform's regions, ended by an entry with a size
 *  and attributes of 0, or NULL for none
 *
 * @return PB_OK, -PB_ERR_PARAM for a board region that breaks a rule, or
 *  -PB_ERR_MEM for a map with too many regions
 */
int pb_mmu_init(const struct mmap_region *plat_mmap);

#endif // INCLUDE_PB_MMU_H
