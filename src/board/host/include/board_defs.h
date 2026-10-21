/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef BOARD_HOST_H
#define BOARD_HOST_H

/* Emulated RAM, mapped at a fixed address by the host platform. Kept below
 * 0x7fff8000, where AddressSanitizer's shadow memory starts on x86-64. */
#define BOARD_RAM_SIZE (512 * 1024 * 1024ul)
#define BOARD_RAM_BASE (0x40000000)
#define BOARD_RAM_END  (BOARD_RAM_BASE + BOARD_RAM_SIZE)

#endif
