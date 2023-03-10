/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_USDHC_H
#define PLAT_IMX_USDHC_H

#include <pb/mmc.h>

struct imx_usdhc_config {
    uintptr_t base;             /*!< Base address of USDHC controller */
    uint64_t clock_freq_hz;     /*!< Input clock of USDHC in Hz */
    const struct mmc_device_config mmc_config; /*!< MMC layer configuration */
};

/**
 * Initialize the usdhc mmc controller
 *
 * @param[in] cfg Pointer to a USDHC configuration struct
 *
 * @return PB_OK on success
 */
int imx_usdhc_init(const struct imx_usdhc_config *cfg);

#endif  // PLAT_IMX_USDHC_H
