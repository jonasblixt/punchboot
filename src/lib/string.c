/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <string.h>
#include <stdint.h>

void* memcpy(void *dest, const void *src, size_t sz) {
    uint8_t *p1 = (uint8_t*) dest;
    uint8_t *p2 = (uint8_t*) src;
    
    for (uint32_t i = 0; i < sz; i++)
        *p1++ = *p2++;

    return dest;
}


