/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 *
 * These functions are used to measure how long the different parts of the
 * boot process takes.
 *
 */

#ifndef INCLUDE_PB_TIMESTAMP_H
#define INCLUDE_PB_TIMESTAMP_H

#include <stdint.h>
#include <config.h>

#ifdef CONFIG_ENABLE_TIMESTAMPING
void ts(const char *description);
/**
 * List all timestamps
 */
void ts_print(void);

/**
 * Get the total boot time, this function will take te start ts from
 * the first timestamp and the end ts from the last timestamp and compute
 * the differeence.
 *
 * @return Total boot time in us
 **/
unsigned int ts_total(void);
#else
#define ts(...)
#define ts_print(...)
#define ts_total(...)
#endif

#endif  // INCLUDE_PB_TIMESTAMP_H_
