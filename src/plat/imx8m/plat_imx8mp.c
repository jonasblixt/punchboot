/**
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * i.MX8M Plus clocks. The ARM PLL is left as the ROM set it. SYSTEM_PLL1
 * (800 MHz) and SYSTEM_PLL2 (1000 MHz) feed the DRAM clocks and most clock
 * roots; all their divided outputs are enabled after reset (RM,
 * CCM_ANALOG_SYS_PLLn_GEN_CTRL).
 */

#include <pb/mmio.h>
#include <pb/pb.h>
#include <plat/imx8m/clock_imx8mp.h>
#include <plat/imx8m/imx8m.h>

#include "plat.h"

/* Start a PLL14xx with its reset dividers, unless it is running already */
static void pll14xx_start(uintptr_t gen_ctrl)
{
    if (mmio_read_32(gen_ctrl) & PLL14XX_LOCK)
        return;

    mmio_clrsetbits_32(gen_ctrl, 0, PLL14XX_RST);

    while (!(mmio_read_32(gen_ctrl) & PLL14XX_LOCK))
        ;

    mmio_clrsetbits_32(gen_ctrl, PLL14XX_BYPASS, 0);
}

void imx8m_soc_clock_init(void)
{
    pll14xx_start(IMX8MP_SYS_PLL1_GEN_CTRL);
    pll14xx_start(IMX8MP_SYS_PLL2_GEN_CTRL);
}
