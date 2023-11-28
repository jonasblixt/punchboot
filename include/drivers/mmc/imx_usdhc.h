/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
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
    const struct mmc_device_config mmc_config; /*!< MMC layer configuration */
};

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
