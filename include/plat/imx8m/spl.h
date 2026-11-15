/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What a board gives the i.MX8M's SPL besides its DRAM (board.md 7, D-088)
 */

#ifndef INCLUDE_PLAT_IMX8M_SPL_H
#define INCLUDE_PLAT_IMX8M_SPL_H

/**
 * The SPL's console [optional]: the board's UART, set up as its
 * console_init() does, with console_init() at its end. The SPL calls it
 * first; its lines, also in the RAM log, are then on the console as well.
 * Without it the SPL writes the RAM log only, which pb proper prints.
 */
void imx8m_spl_console_init(void);

#endif // INCLUDE_PLAT_IMX8M_SPL_H
