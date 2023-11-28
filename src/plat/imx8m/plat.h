/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX8M_PLAT_H_
#define PLAT_IMX8M_PLAT_H_

#define IMX8M_FUSE_BANK_WORD(__b, __w, __d)                                \
    {                                                                      \
        .bank = __b, .word = __w, .description = __d, .status = FUSE_VALID \
    }

#define IMX8M_FUSE_BANK_WORD_VAL(__b, __w, __d, __v)                                             \
    {                                                                                            \
        .bank = __b, .word = __w, .description = __d, .default_value = __v, .status = FUSE_VALID \
    }

#define IMX8M_FUSE_END         \
    {                          \
        .status = FUSE_INVALID \
    }

#define IMX8M_PRIV(__p) ((struct imx8m_private *)__p)

struct imx8m_private {
    uint32_t soc_ver_var;
};

#endif // PLAT_IMX8M_PLAT_H_
