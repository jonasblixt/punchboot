/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This driver is used as a source for systick on imx SoC:s. It runs the GPT
 * as a free running 32-bit counter with a 1 us tick.
 *
 * Note: Don't add any console prints in this drivers since it must be initialize
 * d before the console drivers.
 */

#include <drivers/timer/imx_gpt.h>
#include <pb/errors.h>
#include <pb/mmio.h>

#define GPT_CR                 0x0000
#define GPT_PR                 0x0004
#define GPT_SR                 0x0008
#define GPT_IR                 0x000c
#define GPT_CNT                0x0024

/* Control register */
#define GPT_CR_EN              BIT(0)
#define GPT_CR_ENMOD           BIT(1)
#define GPT_CR_CLKSRC(x)       (((x) & 0x7) << 6)
#define GPT_CR_FRR             BIT(9)
#define GPT_CR_EN_24M          BIT(10)
#define GPT_CR_SWR             BIT(15)

/* CLKSRC values */
#define GPT_CLKSRC_HIGH_FREQ   2 /* ipg_clk_highfreq, perclk_clk_root */
#define GPT_CLKSRC_24M         5 /* ipg_clk_24M, the 24 MHz crystal */

/* Prescaler register. The clocks are divided by the field value + 1 */
#define GPT_PR_PRESCALER(x)    (((x) & 0xfff) << 0)
#define GPT_PR_PRESCALER24M(x) (((x) & 0xf) << 12)
#define GPT_PRESCALER_MAX_DIV  4096

/* Status register, write 1 to clear: OF1-3, IF1-2, ROV */
#define GPT_SR_ALL             0x3f

#define GPT_TICK_Hz            MHz(1)

/*
 * The crystal is divided by 8 before it's synchronized to ahb_clk, which
 * must run at more than twice that rate (RM 28.3), then by 3 to 1 MHz.
 */
#define GPT_24M_PRE_DIV        8
#define GPT_24M_DIV            3

static uintptr_t gpt_base;

/* RM 28.5.1, the sequence for changing the clock source */
static void gpt_start(uintptr_t base, uint32_t clk, uint32_t pr)
{
    gpt_base = base;

    mmio_write_32(gpt_base + GPT_CR, 0);
    mmio_write_32(gpt_base + GPT_IR, 0);
    mmio_write_32(gpt_base + GPT_CR, clk);
    mmio_write_32(gpt_base + GPT_CR, clk | GPT_CR_SWR);

    while (mmio_read_32(gpt_base + GPT_CR) & GPT_CR_SWR) {
        __asm__("nop");
    }

    mmio_write_32(gpt_base + GPT_SR, GPT_SR_ALL);
    /* The software reset clears PR */
    mmio_write_32(gpt_base + GPT_PR, pr);
    mmio_write_32(gpt_base + GPT_CR, clk | GPT_CR_FRR | GPT_CR_ENMOD);
    mmio_write_32(gpt_base + GPT_CR, clk | GPT_CR_FRR | GPT_CR_ENMOD | GPT_CR_EN);
}

int imx_gpt_init(uintptr_t base, unsigned int input_clock_Hz)
{
    unsigned int div = input_clock_Hz / GPT_TICK_Hz;

    if (div == 0 || div > GPT_PRESCALER_MAX_DIV)
        return -PB_ERR_PARAM;

    gpt_start(base, GPT_CR_CLKSRC(GPT_CLKSRC_HIGH_FREQ), GPT_PR_PRESCALER(div - 1));

    return PB_OK;
}

int imx_gpt_init_24m(uintptr_t base)
{
    gpt_start(base,
              GPT_CR_CLKSRC(GPT_CLKSRC_24M) | GPT_CR_EN_24M,
              GPT_PR_PRESCALER24M(GPT_24M_PRE_DIV - 1) | GPT_PR_PRESCALER(GPT_24M_DIV - 1));

    return PB_OK;
}

unsigned int imx_gpt_get_tick(void)
{
    return mmio_read_32(gpt_base + GPT_CNT);
}
