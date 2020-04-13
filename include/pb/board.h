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

int board_pre_boot(void *plat);
int board_late_init(void *plat);
bool board_force_command_mode(void *plat);

int board_jump(const char *boot_part_uu_str);

int board_command(void *plat,
                     uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size);

int board_status(void *plat,
                    void *response_bfr,
                    size_t *response_size);

const char *board_name(void);
#endif  // INCLUDE_PB_BOARD_H_
