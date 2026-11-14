/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef DRIVERS_MMC_IMX_USDHC
#define DRIVERS_MMC_IMX_USDHC

#include <drivers/mmc/mmc_core.h>

struct imx_usdhc_config {
    uintptr_t base; /*!< Base address of USDHC controller */
    unsigned int delay_tap;
    bool card_detect; /*!< CD_B is connected (SD slots) */
    const struct mmc_device_config mmc_config; /*!< MMC layer configuration */
};

/*
 * What the driver leaves behind (bio.md 5), as the code does it today. Its
 * calls are synchronous, so it has no stop. The controller stays enabled as
 * the last call left it: the card clock's dividers for the bus mode, the bus
 * width and DDR_EN, the delay line's tap, ADMA2 selected and ADMA_SYS_ADDR
 * at the driver's descriptor table, which the controller reads only when a
 * command with data is written. Its interrupt signals are off. The card's
 * side is in drivers/mmc/mmc_core.h.
 */

/**
 * Initialize the usdhc mmc controller
 *
 * @param[in] cfg Pointer to a USDHC configuration struct
 * @param[in] intput_clock_hz USDHC input clock (Hz)
 *
 * @return PB_OK on success
 */
int imx_usdhc_init(const struct imx_usdhc_config *cfg, unsigned int input_clock_hz);

#endif
