/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX8X_PLAT_H_
#define PLAT_IMX8X_PLAT_H_

#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>

#define IMX8X_FUSE_ROW(__r, __d) \
        {.bank = __r , .word = 0, .description = __d, .status = FUSE_VALID}

#define IMX8X_FUSE_ROW_VAL(__r, __d, __v) \
        {.bank = __r, .word = 0, .description = __d, \
         .default_value = __v, .status = FUSE_VALID}

#define IMX8X_FUSE_END { .status = FUSE_INVALID }

#define IMX8X_PRIV(__p) ((struct imx8x_private *) __p)

struct imx8x_private
{
    sc_ipc_t ipc;
};

#endif  // PLAT_IMX8X_PLAT_H_
