/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <io.h>
#include <plat/imx6ul/gpt.h>


void gp_timer_init(struct gp_timer *d) {
    d->cr = 0;
    pb_writel(d->cr, d->base+GP_TIMER_CR);
    /* Prescaler: 24MHz / 12 = 2 MHz / 2000 
     *   = 1kHz
     *
     * */


    pb_writel( (1 << 6) | (1 << 15), d->base+GP_TIMER_CR);
    while (pb_readl(d->base+GP_TIMER_CR) & (1<<15))
        asm("nop");
    
    pb_writel(660, d->base+GP_TIMER_PR);
    pb_write( (1 << 6) | (1 << 9), d->base+GP_TIMER_CR);
 
   
    /* Enable timer */
    d->cr = 1 | (2 << 6) | (1 << 9);
    pb_writel(d->cr, d->base+GP_TIMER_CR);
}

u32 gp_timer_get_tick(struct gp_timer *d) {
    return (pb_readl(d->base+GP_TIMER_CNT) / 100);
}


