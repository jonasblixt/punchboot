/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX6UL_PLAT_H_
#define PLAT_IMX6UL_PLAT_H_

#define IMX6UL_FUSE_BANK_WORD(__b, __w, __d)                               \
    {                                                                      \
        .bank = __b, .word = __w, .description = __d, .status = FUSE_VALID \
    }

#define IMX6UL_FUSE_BANK_WORD_VAL(__b, __w, __d, __v)                                            \
    {                                                                                            \
        .bank = __b, .word = __w, .description = __d, .default_value = __v, .status = FUSE_VALID \
    }

#define IMX6UL_FUSE_END        \
    {                          \
        .status = FUSE_INVALID \
    }

#define IMX6UL_PRIV(__p) ((struct imx6ul_private *)__p)

struct imx6ul_private {};

#endif // PLAT_IMX6UL_PLAT_H_
