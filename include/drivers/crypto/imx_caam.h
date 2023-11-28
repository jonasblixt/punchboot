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

#endif // DRIVERS_CRYPTO_IMX_CAAM_H
