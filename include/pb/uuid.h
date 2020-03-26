/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_UUID_H_
#define INCLUDE_PB_UUID_H_

#include <stdint.h>

uint32_t uuid_to_string(const uint8_t *uuid, char *out);
uint32_t uuid_gen_uuid3(const char *ns,
                        uint32_t ns_length,
                        const char *unique_data,
                        uint32_t unique_data_length,
                        char *out);
uint32_t uuid_to_guid(const uint8_t *uuid, uint8_t *guid);

#endif  // INCLUDE_PB_UUID_H_
