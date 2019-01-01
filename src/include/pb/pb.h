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
};

#if LOGLEVEL >= 1
    #define LOG_INFO(...) \
        do { tfp_printf("INFO %s: " , __func__);\
             tfp_printf(__VA_ARGS__);\
             tfp_printf("\n\r"); } while(0)

    #define LOG_INFO2(...) \
        do { tfp_printf ("INFO: ");\
             tfp_printf(__VA_ARGS__); } while(0)
#else
    #define LOG_INFO(...)
    #define LOG_INFO2(...)
#endif

#define LOG_WARN(...) \
    do { tfp_printf("WARN %s: " , __func__);\
         tfp_printf(__VA_ARGS__);\
         tfp_printf("\n\r"); } while(0)

#define LOG_ERR(...) \
    do { tfp_printf("ERROR %s: " , __func__);\
         tfp_printf(__VA_ARGS__);\
         tfp_printf("\n\r"); } while(0)

#define UNUSED(x) (void)(x)

typedef volatile uint32_t __iomem;

#define __no_bss __attribute__((section (".bigbuffer")))
#define __a4k  __attribute__ ((aligned(4096)))
#ifndef __packed
    #define __packed __attribute__ ((packed))
#endif
#endif


