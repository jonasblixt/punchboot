/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef TOOLS_PUNCHBOOT_CRC_H_
#define TOOLS_PUNCHBOOT_CRC_H_

#include <sys/types.h>

u_int32_t crc32(u_int32_t crc, const void *buf, size_t size);

#endif  // TOOLS_PUNCHBOOT_CRC_H_
