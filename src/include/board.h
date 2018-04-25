/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __BOARD_H_
#define __BOARD_H_

#include "pb_types.h"

u32 board_init(void);
u8 board_force_recovery(void);
u32 board_usb_init(void);

#endif
