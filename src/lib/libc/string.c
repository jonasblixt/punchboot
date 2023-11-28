/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdint.h>
#include <string.h>

void *memcpy(void *dest, const void *src, size_t sz)
{
    uint8_t *p1 = (uint8_t *)dest;
    uint8_t *p2 = (uint8_t *)src;

    for (uint32_t i = 0; i < sz; i++)
        *p1++ = *p2++;

    return dest;
}

char *strncpy(char *dest, const char *src, size_t n)
{
    size_t i;

    for (i = 0; i < n && src[i] != '\0'; i++)
        dest[i] = src[i];
    for (; i < n; i++)
        dest[i] = '\0';

    return dest;
}
