/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */



#ifndef __BOARD_CONFIG__
#define __BOARD_CONFIG__

#define PB_MAX_FAIL_BOOT_COUNT 16


void test_board_boot(struct pb_pbi *pbi, uint8_t system_index);

#define PB_BOOT_FUNC(x,y) test_board_boot(x,y)

#endif
