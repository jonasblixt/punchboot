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

#include <drivers/timer/imx_gpt.h>
#include <pb/errors.h>
#include <pb/mmio.h>

#define GPT_CR               0x0000
#define GPT_PR               0x0004
#define GPT_SR               0x0008
#define GPT_CNT              0x0024

/* Control register */
#define GPT_CR_EN            BIT(0)
#define GPT_CR_CLKSRC(x)     (((x) & 0x7) << 6)
#define GPT_CR_FRR           BIT(9)
#define GPT_CR_EN_24M        BIT(10)
#define GPT_CR_SWR           BIT(15)

/* CLKSRC values */
#define GPT_CLKSRC_HIGH_FREQ 2 /* ipg_clk_highfreq, perclk_clk_root */

#define GPT_TICK_Hz          MHz(1)

static uintptr_t gpt_base;

int imx_gpt_init(uintptr_t base, unsigned int input_clock_Hz)
{
    uint32_t cr = 0;
    gpt_base = base;

    mmio_write_32(gpt_base + GPT_CR, GPT_CR_SWR);

    while (mmio_read_32(gpt_base + GPT_CR) & GPT_CR_SWR) {
        __asm__("nop");
    }

    /* The counter clock is divided by PR + 1 */
    mmio_write_32(gpt_base + GPT_PR, input_clock_Hz / GPT_TICK_Hz - 1);

    cr = GPT_CR_CLKSRC(GPT_CLKSRC_HIGH_FREQ) | GPT_CR_FRR | GPT_CR_EN_24M;
    mmio_write_32(gpt_base + GPT_CR, cr);

    /* Enable timer */
    cr |= GPT_CR_EN;

    mmio_write_32(gpt_base + GPT_CR, cr);

    return PB_OK;
}

unsigned int imx_gpt_get_tick(void)
{
    return mmio_read_32(gpt_base + GPT_CNT);
}
