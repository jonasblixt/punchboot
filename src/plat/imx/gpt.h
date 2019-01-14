/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef __PLAT_IMX6UL_GPT__
#define __PLAT_IMX6UL_GPT__

#include <pb.h>

#define GP_TIMER_CR  0x0000
#define GP_TIMER_PR  0x0004
#define GP_TIMER_SR  0x0008
#define GP_TIMER_CNT 0x0024

struct gp_timer 
{
    __iomem base;
    uint32_t pr;
    uint32_t cr;
};

uint32_t gp_timer_init(struct gp_timer *d);
uint32_t gp_timer_get_tick(struct gp_timer *d);

#endif
