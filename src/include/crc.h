/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __CRC_H__
#define __CRC_H__

#include "pb_types.h"

u32 crc32(u32 crc, const u8 *buf, u32 size);


#endif
