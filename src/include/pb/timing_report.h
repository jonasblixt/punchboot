
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef INCLUDE_PB_TIMING_REPORT_H_
#define INCLUDE_PB_TIMING_REPORT_H_

#include <stdint.h>

#define TR_NO 6

enum tr_kinds
{
    TR_POR = 0,
    TR_BLINIT = 1,
    TR_LOAD = 2,
    TR_VERIFY = 3,
    TR_DT_PATCH = 4,
    TR_TOTAL = 5,
};

void tr_init(void);
void tr_stamp_begin(uint32_t kind);
void tr_stamp_end(uint32_t kind);
void tr_print_result(void);

#endif  // INCLUDE_PB_TIMING_REPORT_H_
