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

#define __no_bss __attribute__((section (".bigbuffer")))
#define __a4k  __attribute__ ((aligned(4096)))
#define __a16b  __attribute__ ((aligned(16)))

#endif


