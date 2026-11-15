/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_PICO8ML_CONSOLE_H
#define BOARD_PICO8ML_CONSOLE_H

/* UART1 at 115200, the console (board.c's console_init, the SPL's) */
void pico8ml_console_init(void);

#endif // BOARD_PICO8ML_CONSOLE_H
