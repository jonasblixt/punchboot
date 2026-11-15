/**
 * Punch BOOT
 *
 * Copyright (C) 2020, 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * The pico8ml's DRAM, in both stages (board.md 7, D-088): its rates and its
 * density straps; the SPL also chooses the timing by them.
 */

#include <drivers/gpio/imx_gpio.h>
#include <drivers/pinctrl/imx_iomuxc.h>
#include <pb/pb.h>
#include <plat/imx8m/dram.h>
#include <plat/imx8m/imx8m.h>

#include "dram.h"
#include "lpddr4_timing.h"

/* DRAM: both set points of the timings, for DDR frequency scaling in Linux */
static const unsigned int pico8ml_dram_mts[] = { 3200, 667 };

struct dram_rates dram_rates = {
    .mts = pico8ml_dram_mts,
    .num = ARRAY_SIZE(pico8ml_dram_mts),
};

#define IMX8MQ_GPIO1_BASE 0x30200000
#define DDR_DET_BANK      2 /* GPIO3 */
#define DDR_DET_1         11
#define DDR_DET_2         12
#define DDR_DET_3         13

/*
 * The density straps (TechNexion's spl.c): DDR_DET_1, _2 and _3 on
 * GPIO3_IO11, _12 and _13, the pads NAND_DATA05 to 07.
 *
 *   DDR_DET_1  DDR_DET_2  DDR_DET_3
 *       0          0          1      4 GiB
 *       1          1          1      3 GiB
 *       1          1          0      2 GiB
 *       1          0          1      1 GiB
 *
 * pb itself uses the first GiB (BOARD_DRAM_SIZE, D-079), what every
 * variant has.
 */
static unsigned int dram_straps(void)
{
    static const struct imx_pin pins[] = {
        IMX8M_PIN(NAND_DATA05, MUX_ALT(5), IMX_PAD_KEEP),
        IMX8M_PIN(NAND_DATA06, MUX_ALT(5), IMX_PAD_KEEP),
        IMX8M_PIN(NAND_DATA07, MUX_ALT(5), IMX_PAD_KEEP),
    };

    imx_iomuxc_setup(pins, ARRAY_SIZE(pins));
    imx_gpio_init(IMX8MQ_GPIO1_BASE);

    return (imx_gpio_get_pin(DDR_DET_BANK, DDR_DET_1) << 2) |
           (imx_gpio_get_pin(DDR_DET_BANK, DDR_DET_2) << 1) |
           imx_gpio_get_pin(DDR_DET_BANK, DDR_DET_3);
}

unsigned int pico8ml_dram_gib(void)
{
    switch (dram_straps()) {
    case 0x1:
        return 4;
    case 0x7:
        return 3;
    case 0x6:
        return 2;
    case 0x5:
        return 1;
    default:
        return 0;
    }
}

#ifdef PB_SPL
const struct dram_timing_info *imx8m_board_dram_timing(void)
{
    unsigned int straps = dram_straps();

    switch (straps) {
    case 0x1:
        return &dram_timing_4gb;
    case 0x7:
        return &dram_timing_3gb;
    case 0x6:
        return &dram_timing_2gb;
    case 0x5:
        return &dram_timing_1gb;
    default:
        LOG_ERR("Unknown DRAM density straps 0x%x", straps);
        return NULL;
    }
}
#endif
