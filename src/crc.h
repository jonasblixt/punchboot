/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_CRC_H_
#define INCLUDE_PB_CRC_H_

#include <stdint.h>

uint32_t crc32(uint32_t crc, const uint8_t *buf, uint32_t size);

#endif  // INCLUDE_PB_CRC_H_
