/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This driver is used as a source for systick on imx SoC:s, the driver
 * will try to produce a 1us tick based on the input clock.
 *
 * Note: Don't add any console prints in this drivers since it must be initialize
 * d before the console drivers.
 */

#include <pb/mmio.h>
#include <pb/errors.h>
#include <drivers/timer/imx_gpt.h>

#define GP_TIMER_CR  0x0000
#define GP_TIMER_PR  0x0004
#define GP_TIMER_SR  0x0008
#define GP_TIMER_CNT 0x0024

static uintptr_t gpt_base;

int imx_gpt_init(uintptr_t base, unsigned int input_clock_Hz)
{
    uint32_t cr = 0;
    gpt_base = base;

    mmio_write_32(gpt_base + GP_TIMER_CR, (1 << 15));

    while (mmio_read_32(gpt_base + GP_TIMER_CR) & (1<<15)) {
        __asm__("nop");
    }

    mmio_write_32(gpt_base + GP_TIMER_PR, input_clock_Hz / MHz(1));

    cr = (2 << 6) | (1 << 9) | (1 << 10);
    mmio_write_32(gpt_base + GP_TIMER_CR, cr);

    /* Enable timer */
    cr |= 1;

    mmio_write_32(gpt_base + GP_TIMER_CR, cr);

    return PB_OK;
}

unsigned int imx_gpt_get_tick(void)
{
    return mmio_read_32(gpt_base + GP_TIMER_CNT);
}


