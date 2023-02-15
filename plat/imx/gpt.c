/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/io.h>
#include <pb/pb.h>
#include <plat/imx/gpt.h>
#include <plat/defs.h>

int gp_timer_init(void)
{
    uint32_t cr = 0;

    pb_write32(1<<15, IMX_GPT_BASE + GP_TIMER_CR);

    while (pb_read32(IMX_GPT_BASE + GP_TIMER_CR) & (1<<15)) {
        __asm__("nop");
    }

    pb_write32(IMX_GPT_PR, IMX_GPT_BASE + GP_TIMER_PR);

    cr = (2 << 6) | (1 << 9) | (1 << 10);
    pb_write32(cr, IMX_GPT_BASE + GP_TIMER_CR);

    /* Enable timer */
    cr |= 1;

    pb_write32(cr, IMX_GPT_BASE + GP_TIMER_CR);

    return PB_OK;
}

unsigned int gp_timer_get_tick(void)
{
    return (pb_read32(IMX_GPT_BASE + GP_TIMER_CNT));
}


