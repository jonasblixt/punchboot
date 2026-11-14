/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonas@blxt.se>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_DRIVERS_FUSE_TEST_BIO_H
#define INCLUDE_DRIVERS_FUSE_TEST_BIO_H

#include <pb/bio.h>

int test_fuse_init(bio_dev_t dev);
int test_fuse_write(uint32_t addr, uint32_t value);
int test_fuse_read(uint32_t addr, uint32_t *value);

#endif // INCLUDE_DRIVERS_FUSE_TEST_BIO_H
