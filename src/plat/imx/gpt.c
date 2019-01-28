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
    d->cr = 0;
    pb_write32(d->cr, d->base+GP_TIMER_CR);

    /* Prescaler: 24MHz / 12 = 2 MHz / 2000 
     *   = 1kHz
     *
     * */


    d->cr = (2 << 6) | (1 << 15) | (1 << 10);
    pb_write32(d->cr, d->base+GP_TIMER_CR);

    while (pb_read32(d->base+GP_TIMER_CR) & (1<<15))
        __asm__("nop");
 
    d->cr &= ~(1 << 15); 
    pb_write32(d->pr, d->base+GP_TIMER_PR);

    d->cr |= (1 << 9);

    pb_write16(d->cr, d->base+GP_TIMER_CR);
 
   
    /* Enable timer */
    d->cr |= 1;

    /**
     * (5 << 6) Selects 24 MHz xtal as source
     *
     *
     * Timer freq = 25MHz / (Prescaler + 1) =
     *  = 24e6 / (23+1) = 1 MHz
     *
     */

    pb_write32(d->cr, d->base+GP_TIMER_CR);

    return PB_OK;
}

uint32_t gp_timer_get_tick(struct gp_timer *d) 
{
    return (pb_read32(d->base+GP_TIMER_CNT));
}


