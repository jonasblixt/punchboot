/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_BOOT_H_
#define INCLUDE_PB_BOOT_H_

#include <stdint.h>
#include <image.h>
#include <gpt.h>


void pb_boot(struct pb_pbi *pbi, uint32_t system_index, bool verbose);

#endif  // INCLUDE_PB_BOOT_H_
