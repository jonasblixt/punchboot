/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * What the SoC is and how it runs, from plat_get_info(), in the boot log and
 * as the named command 'info' (doc/interfaces/platform.md 3.11, D-077).
 */

#ifndef INCLUDE_PB_PLAT_INFO_H
#define INCLUDE_PB_PLAT_INFO_H

#include <stddef.h>

/** Log the SoC: one line, then a line per specific. Called by main() after plat_init() */
void plat_info_log(void);

/**
 * The SoC as text: the line plat_info_log() logs and the specifics, a line
 * each, NUL terminated. What 'info' answers
 *
 * @param[out] out The text
 * @param[in] size The room in out, with the NUL
 *
 * @return The text's length without the NUL, or the error of plat_get_info()
 */
int plat_info_text(char *out, size_t size);

#endif // INCLUDE_PB_PLAT_INFO_H
