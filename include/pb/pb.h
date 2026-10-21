/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_PB_H
#define INCLUDE_PB_PB_H

#include <config.h>
#include <inttypes.h>
#include <pb/errors.h>
#include <pb/log_def.h>
#include <pb/utils_def.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef CONFIG_ARCH_HOST
/* Host builds rename main (see CMakeLists.txt), the process entry point is
 * provided by the host platform */
void pb_main(void);
#else
void main(void);
#endif

#endif // INCLUDE_PB_PB_H
