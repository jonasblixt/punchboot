/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __PB_H__
#define __PB_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum {
    PB_OK,
    PB_ERR,
    PB_TIMEOUT,
};

struct partition_table
{
    uint64_t no_of_blocks;
    const char uuid[37];
    const char name[20];
};

#define PB_GPT_ENTRY(__blks, __uuid, __name) \
        {.no_of_blocks = __blks, .uuid = __uuid, .name = __name}

#define PB_GPT_END {.no_of_blocks = 0, .uuid = "", .name = ""}

#if LOGLEVEL >= 2
    #define LOG_INFO(...) \
        do { printf("INFO %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while(0)

    #define LOG_INFO2(...) \
        do { printf ("INFO: ");\
             printf(__VA_ARGS__); } while(0)
#else
    #define LOG_INFO(...)
    #define LOG_INFO2(...)
#endif

#if LOGLEVEL >= 3
    #define LOG_DBG(...) \
        do { printf("DBG %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while(0)
#else
    #define LOG_DBG(...)
#endif

#if LOGLEVEL >= 1
    #define LOG_WARN(...) \
        do { printf("WARN %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while(0)

    #define LOG_ERR(...) \
        do { printf("ERROR %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while(0)
#else
    #define LOG_WARN(...)
    #define LOG_ERR(...)
#endif

#define UNUSED(x) (void)(x)

#define PB_CHECK_OVERLAP(__a,__sz,__region_start, __region_end) \
    (( __a >= ((uintptr_t) __region_start) ) && \
    ( __a <= ((uintptr_t) __region_end) )) || \
    (( (__a + __sz) >= ((uintptr_t) __region_start) ) && \
    ( (__a + __sz) <= ((uintptr_t) __region_end) ))

#define __no_bss __attribute__((section (".bigbuffer")))
#define __a4k  __attribute__ ((aligned(4096)))
#define __a16b  __attribute__ ((aligned(16)))

/*
    System A: 2af755d8-8de5-45d5-a862-014cfa735ce0
    System B: c046ccd8-0f2e-4036-984d-76c14dc73992
    Root A:   c284387a-3377-4c0f-b5db-1bcbcff1ba1a
    Root B:   ac6a1b62-7bd0-460b-9e6a-9a7831ccbfbb
*/

#ifndef PB_PARTUUID_SYSTEM_A
    #define PB_PARTUUID_SYSTEM_A "\x2a\xf7\x55\xd8\x8d\xe5\x45\xd5\xa8\x62\x01\x4c\xfa\x73\x5c\xe0"
#endif

#ifndef PB_PARTUUID_SYSTEM_B
    #define PB_PARTUUID_SYSTEM_B "\xc0\x46\xcc\xd8\x0f\x2e\x40\x36\x98\x4d\x76\xc1\x4d\xc7\x39\x92"
#endif

#ifndef PB_PARTUUID_ROOT_A
    #define PB_PARTUUID_ROOT_A "\xc2\x84\x38\x7a\x33\x77\x4c\x0f\xb5\xdb\x1b\xcb\xcf\xf1\xba\x1a"
#endif

#ifndef PB_PARTUUID_ROOT_B
    #define PB_PARTUUID_ROOT_B "\xac\x6a\x1b\x62\x7b\xd0\x46\x0b\x9e\x6a\x9a\x78\x31\xcc\xbf\xbb"
#endif

#endif


