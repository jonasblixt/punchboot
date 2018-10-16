/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <string.h>

void* memcpy(void *dest, const void *src, size_t sz) {
    uint8_t *p1 = (uint8_t*) dest;
    uint8_t *p2 = (uint8_t*) src;
    
    for (uint32_t i = 0; i < sz; i++)
        *p1++ = *p2++;

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    uint8_t *p1 = (uint8_t*) s1;
    uint8_t *p2 = (uint8_t*) s2;
    
    for (uint32_t i = 0; i < n; i++)
        if (*p1++ != *p2++)
            return -1;
    return 0;
}

