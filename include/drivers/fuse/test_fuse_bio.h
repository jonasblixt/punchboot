/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_FUSE_TEST_BIO_H
#define INCLUDE_DRIVERS_FUSE_TEST_BIO_H

#include <pb/bio.h>

int test_fuse_init(bio_dev_t dev);
int test_fuse_write(uint16_t addr, uint32_t value);
int test_fuse_read(uint16_t addr);

#endif // INCLUDE_DRIVERS_FUSE_TEST_BIO_H
