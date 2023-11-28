/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_FUSE_IMX_OCOTP_H
#define INCLUDE_DRIVERS_FUSE_IMX_OCOTP_H

#include <stdint.h>

#define OCOTP_CTRL           0x0000
#define OCOTP_TIMING         0x0010
#define OCOTP_DATA           0x0020
#define OCOTP_READ_CTRL      0x0030
#define OCOTP_READ_FUSE_DATA 0x0040

/* Control register */
#define OCOTP_CTRL_BUSY      (1 << 8)
#define OCOTP_CTRL_ERROR     (1 << 9)

#define OCOTP_CTRL_WR_KEY    0x3E77

int imx_ocotp_init(uintptr_t base, unsigned int words_per_bank);
int imx_ocotp_read(uint32_t bank, uint32_t row, uint32_t *value);
int imx_ocotp_write(uint32_t bank, uint32_t row, uint32_t value);

#endif // INCLUDE_DRIVERS_FUSE_IMX_OCOTP_H
