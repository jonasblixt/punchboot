/**
 * Punch BOOT bootloader cli
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef TOOLS_PUNCHBOOT_UTILS_H_
#define TOOLS_PUNCHBOOT_UTILS_H_

#include <stdint.h>
#include <pb/gpt.h>

void utils_gpt_part_name(struct gpt_part_hdr *part, uint8_t *out, uint8_t len);

#endif  // TOOLS_PUNCHBOOT_UTILS_H_
