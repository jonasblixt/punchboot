/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The board that src/mmu.c reads, for the ports' tests and test_mmu: nothing
 * but 'mmap' (board.md 2.8), which is fake_board_mmap, a list the test fills
 * and that is empty as it starts (zeroes: PB_MAP_END). Built with
 * FAKE_BOARD_NO_MMAP, 'mmap' is NULL.
 */

#include <pb/board.h>

#include "xlat.h"

struct pb_mmap_region fake_board_mmap[FAKE_BOARD_MMAP_MAX];

const struct board board = {
#ifndef FAKE_BOARD_NO_MMAP
    .mmap = fake_board_mmap,
#endif
};
