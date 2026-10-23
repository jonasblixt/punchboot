/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_GPT_H_
#define PLAT_IMX_GPT_H_

#include <pb/pb.h>

/**
 * Start the GPT with a 1 us tick from the high frequency reference clock
 * (ipg_clk_highfreq, which is perclk_clk_root on most SoC:s)
 *
 * @param[in] base GPT base address
 * @param[in] input_clock_Hz Rate of ipg_clk_highfreq, 1 MHz to 4096 MHz.
 *            The tick is off by the remainder of a rate that isn't whole MHz.
 *
 * @return PB_OK on success, -PB_ERR_PARAM for a rate out of range
 */
int imx_gpt_init(uintptr_t base, unsigned int input_clock_Hz);

/**
 * Start the GPT with a 1 us tick from the 24 MHz crystal (ipg_clk_24M). The
 * tick doesn't depend on how the boot ROM set up the peripheral clocks.
 *
 * @param[in] base GPT base address
 *
 * @return PB_OK on success
 */
int imx_gpt_init_24m(uintptr_t base);

unsigned int imx_gpt_get_tick(void);

#endif // PLAT_IMX_GPT_H_
