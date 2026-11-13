/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef DRIVERS_CRYPTO_IMX_CAAM_H
#define DRIVERS_CRYPTO_IMX_CAAM_H

#include <stdint.h>

int imx_caam_init(uintptr_t base);

/**
 * Instantiate the CAAM's RNG4 state handles, with prediction resistance, and
 * generate the secure keys (JDKEK, TDKEK, TDSK)
 *
 * The state handles are instantiated with prediction resistance because Linux
 * tears down one without, and when Linux runs in the normal world it can't
 * instantiate them again. Handles that are already instantiated with it (by
 * the ROM, or by pb before a warm reset) are left alone. Needs secure access
 * to the controller page, and runs on the job ring imx_caam_init() set up.
 *
 * @param[in] ctrl_base The CAAM's controller page (page 0)
 *
 * @return PB_OK on success, or if the CAAM has no RNG4,
 *        -PB_ERR_STATE if imx_caam_init() hasn't run,
 *        -PB_ERR_NOT_SUPPORTED on an unexpected number of state handles,
 *        -PB_ERR_TIMEOUT if a job times out,
 *        -PB_ERR if the state handles aren't instantiated at the longest
 *        entropy delay
 */
int imx_caam_rng_init(uintptr_t ctrl_base);

#endif // DRIVERS_CRYPTO_IMX_CAAM_H
