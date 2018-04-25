/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <pb_string.h>

void* memcpy(void *dest, const void *src, size_t sz) {
    u8 *p1 = (u8*) dest;
    u8 *p2 = (u8*) src;
    
    for (u32 i = 0; i < sz; i++)
        *p1++ = *p2++;

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    u8 *p1 = (u8*) s1;
    u8 *p2 = (u8*) s2;
    
    for (u32 i = 0; i < n; i++)
        if (*p1++ != *p2++)
            return -1;
    return 0;
}


