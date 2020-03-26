/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_FUSE_H_
#define INCLUDE_PB_FUSE_H_

#include <stdint.h>

#define FUSE_INVALID 0
#define FUSE_VALID 1

#define foreach_fuse_read(__f, __var) \
    for (struct fuse *__f = (struct fuse *)__var; \
        (plat_fuse_read(__f) == PB_OK); __f++)

#define foreach_fuse(__f, __var) \
    for (struct fuse *__f = (struct fuse *)__var; \
        ((__f->status & FUSE_VALID) == FUSE_VALID); __f++)

struct fuse
{
    uint32_t bank;
    uint32_t word;
    uint32_t shadow;
    uint32_t addr;
    volatile uint32_t value;
    uint32_t default_value;
    uint32_t status;
    const char description[20];
};

#endif  // INCLUDE_PB_FUSE_H_
