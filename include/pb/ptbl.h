/**
 * \file ptbl.h
 *
 * Punch BOOT
 *
 * Copyright (C) 2026 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * A board's partition table (board.ptbl): the table driver's functions and
 * the board's data for it, made by the driver's macro: PTBL_GPT()
 * (drivers/partition/gpt.h), PTBL_STATIC() and PTBL_STATIC_MAPPED()
 * (drivers/partition/static.h).
 */

#ifndef INCLUDE_PB_PTBL_H
#define INCLUDE_PB_PTBL_H

#include <pb/bio.h>
#include <stddef.h>
#include <stdint.h>

struct ptbl;

struct ptbl_ops {
    /** Create the partitions of 'ptbl' on its device, 'dev' */
    int (*init)(const struct ptbl *ptbl, bio_dev_t dev);
};

struct ptbl {
    const struct ptbl_ops *ops; /*!< NULL: no table of the core's */
    const unsigned char *device; /*!< UUID of the device the table is on */
    const void *table; /*!< The driver's: layouts or partitions */
    size_t length; /*!< A GPT: the number of layouts */
    uintptr_t base; /*!< A mapped device: the address of its first byte */
};

#endif // INCLUDE_PB_PTBL_H
