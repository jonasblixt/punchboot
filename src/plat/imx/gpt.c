/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <io.h>
#include <pb.h>
#include <plat/imx/gpt.h>

uint32_t gp_timer_init(struct gp_timer *d) 
{
    pb_write32(1<<15, d->base+GP_TIMER_CR);

    while (pb_read32(d->base+GP_TIMER_CR) & (1<<15))
        __asm__("nop");
 
    pb_write32(d->pr, d->base+GP_TIMER_PR);


    d->cr = (2 << 6) | (1 << 9) | (1 << 10);
    pb_write32(d->cr, d->base+GP_TIMER_CR);
 
   
    /* Enable timer */
    d->cr |= 1;

    pb_write32(d->cr, d->base+GP_TIMER_CR);

    return PB_OK;
}

uint32_t gp_timer_get_tick(struct gp_timer *d) 
{
    return (pb_read32(d->base+GP_TIMER_CNT));
}


