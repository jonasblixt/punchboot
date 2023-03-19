/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_PB_H_
#define INCLUDE_PB_PB_H_

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <pb/errors.h>
#include <config.h>

enum pb_slc_state
{
    PB_SECURITY_STATE_NOT_SECURE,
    PB_SECURITY_STATE_CONFIGURED_ERR,
    PB_SECURITY_STATE_CONFIGURED_OK,
    PB_SECURITY_STATE_SECURE,
};

#define PB_HASH_MAX_LENGTH 64

enum pb_hash_algs
{
    PB_HASH_INVALID,
    PB_HASH_MD5,
    PB_HASH_SHA256,
    PB_HASH_SHA384,
    PB_HASH_SHA512,
    PB_HASH_MD5_BROKEN,
};

#define PB_HASH_MD5_LEN 16
#define PB_HASH_SHA256_LEN 32
#define PB_HASH_SHA384_LEN 48
#define PB_HASH_SHA512_LEN 64

#if LOGLEVEL >= 2
    #define LOG_INFO(...) \
        do { printf("I %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)

    #define LOG_INFO2(...) \
        do { printf("I: ");\
             printf(__VA_ARGS__); } while (0)
#else
    #define LOG_INFO(...)
    #define LOG_INFO2(...)
#endif

#if LOGLEVEL >= 3
    #define LOG_DBG(...) \
        do { printf("D %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)
#else
    #define LOG_DBG(...)
#endif

#if LOGLEVEL >= 1
    #define LOG_WARN(...) \
        do { printf("W %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)

    #define LOG_ERR(...) \
        do { printf("E %s: " , __func__);\
             printf(__VA_ARGS__);\
             printf("\n\r"); } while (0)
#else
    #define LOG_WARN(...)
    #define LOG_ERR(...)
#endif

#define UNUSED(x) (void)(x)

#define PB_CHECK_OVERLAP(__a, __sz, __region_start, __region_end) \
    (((__a) <= ((uintptr_t) (__region_end))) &&                 \
     ((__a) + (__sz) >= ((uintptr_t) (__region_start))))

#define PB_SECTION_NO_INIT __attribute__((section (".no_init")))
#define PB_ALIGN_4k  __attribute__ ((aligned(4096)))
#define PB_ALIGN(x)  __attribute__ ((aligned(x)))

#define membersof(array) (sizeof(array) / sizeof((array)[0]))
#define __DECONST(type, var)    ((type)(uintptr_t)(const void *)(var))

#define SZ_kB(x) ((size_t) (x) << 10)
#define SZ_MB(x) ((size_t) (x) << 20)
#define SZ_GB(x) ((size_t) (x) << 30)
#define MHz(x) (x * 1000000UL)

void pb_main(void);

#endif  // INCLUDE_PB_PB_H_
