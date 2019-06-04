
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __TEST_PLAT_H__
#define __TEST_PLAT_H__

#define TEST_FUSE_BANK_WORD(__b, __d) \
        {.bank = __b , .description = __d, .status = FUSE_VALID}

#define TEST_FUSE_BANK_WORD_VAL(__b,__d,__v) \
        {.bank = __b , .description = __d, \
         .default_value = __v, .status = FUSE_VALID}

#define TEST_FUSE_END { .status = FUSE_INVALID }

#endif
