/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BOARD_H_
#define INCLUDE_PB_BOARD_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
/**
 * Function: board_early_init
 *
 * Called as early as possible by platform init code
 *
 * Return value: PB_OK if there is no error
 */

int board_early_init(void *plat);

int board_command(void *plat,
                     uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size);

int board_status(void *plat,
                    void *response_bfr,
                    size_t *response_size);

int board_slc_set_configuration(void *plat);
int board_slc_set_configuration_lock(void *plat);
int board_command_mode_auth(char *password, size_t length);
const char *board_name(void);
void board_command_mode_enter(void);

#endif  // INCLUDE_PB_BOARD_H_
