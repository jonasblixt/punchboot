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

#ifndef INCLUDE_PB_TIMESTAMP_H_
#define INCLUDE_PB_TIMESTAMP_H_

#include <stdint.h>

#define PB_TIMESTAMP_NO_OF_TS 16

/**
 * Start a time measurement
 *
 * @param description[in] Description of the timestamp
 *
 * @return PB_OK on success or a negative nummber
 */
int pb_timestamp_begin(const char *description);

/**
 * Stop current measurement
 *
 * @return PB_OK on success or a negative nummber
 */
int pb_timestamp_end(void);

/**
 * List all timestamps
 */
void pb_timestamp_print(void);

/**
 * Get the total boot time, this function will take te start ts from
 * the first timestamp and the end ts from the last timestamp and compute
 * the differeence.
 *
 * @return Total boot time in us
 **/
int pb_timestamp_total(void);

#endif  // INCLUDE_PB_TIMESTAMP_H_
