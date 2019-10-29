/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_TEST_PL061_H_
#define PLAT_TEST_PL061_H_

#include <pb.h>
#include <io.h>

void pl061_init(__iomem base);
void pl061_configure_direction(uint8_t pin, uint8_t dir);
void pl061_set_value(uint8_t pin, uint8_t value);

#endif  // PLAT_TEST_PL061_H_
