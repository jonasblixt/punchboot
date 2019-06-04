
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef __PL061_H__
#define __PL061_H__

#include <pb.h>
#include <io.h>

void pl061_init(__iomem base);
void pl061_configure_direction(uint8_t pin, uint8_t dir);
void pl061_set_value(uint8_t pin, uint8_t value);

#endif
