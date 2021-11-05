/**
 * Punch BOOT
 *
 * Copyright (C) 2021 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb/io.h>
#include <pb/pb.h>
#include <plat/imx/gpt.h>

static __iomem base;

int gp_timer_init(__iomem base_addr, unsigned int pr)
{
    uint32_t cr = 0;
    base = base_addr;

    pb_write32(1<<15, base + GP_TIMER_CR);

    while (pb_read32(base + GP_TIMER_CR) & (1<<15)) {
        __asm__("nop");
    }

    pb_write32(pr, base + GP_TIMER_PR);

    cr = (2 << 6) | (1 << 9) | (1 << 10);
    pb_write32(cr, base + GP_TIMER_CR);

    /* Enable timer */
    cr |= 1;

    pb_write32(cr, base + GP_TIMER_CR);

    return PB_OK;
}

unsigned int gp_timer_get_tick(void)
{
    return (pb_read32(base + GP_TIMER_CNT));
}


