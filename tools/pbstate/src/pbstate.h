/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef TOOLS_pbstate_pbstate_H_
#define TOOLS_pbstate_pbstate_H_

#include <stdint.h>

#define SYSTEM_NONE 0
#define SYSTEM_A 1
#define SYSTEM_B 2

void print_configuration(void);

uint32_t pbstate_load(const char *device, uint64_t primary_offset,
                        uint64_t backup_offset);
uint32_t pbstate_switch(uint8_t system, uint8_t counter);

uint32_t pbstate_set_verified(uint8_t system);

#endif  // TOOLS_pbstate_pbstate_H_
