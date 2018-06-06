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

#include <pb.h>

uint32_t crc32(uint32_t crc, const uint8_t *buf, uint32_t size);


#endif
