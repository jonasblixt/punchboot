/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_PLAT_H_
#define INCLUDE_PB_PLAT_H_

#include <pb/pb.h>
#include <pb-tools/wire.h>

/* Platform API Calls */

/**
 * Initialize the platform
 *
 * @return PB_OK on success or a negative number
 **/
int plat_init(void);

int plat_board_init(void);

/**
 * Platform specific reset function
 **/
void plat_reset(void);

/**
 * Read platform/system tick
 *
 * @return current micro second tick
 **/
unsigned int plat_get_us_tick(void);

/**
 * Kick watchdog
 */
void plat_wdog_kick(void);

/**
 * Returns a platform specific boot reason as an integer.
 *
 * @return >= 0 for valid boot reasons or a negative number on error
 *
 **/
int plat_boot_reason(void);

/**
 * Returns a platform specific boot reason as an string
 *
 * @return Boot reason string or ""
 *
 **/
const char* plat_boot_reason_str(void);

int plat_get_unique_id(uint8_t *output, size_t *length);

#endif  // INCLUDE_PB_PLAT_H_
