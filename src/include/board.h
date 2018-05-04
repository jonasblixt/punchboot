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

struct board_info {
    u16 type;
    u8 rev;
    u8 var;
};

#define BOARD_OTP_WRITE_KEY 0xaabbccdd

u32 board_init(void);
u8 board_force_recovery(void);
u32 board_usb_init(void);

u32 board_get_uuid(u8 *uuid);
u32 board_get_boardinfo(struct board_info *info);

u32 board_write_uuid(u8 *uuid, u32 key);
u32 board_write_boardinfo(struct board_info *info, u32 key);
u32 board_get_default_part();

#endif
