/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#ifndef PLAT_IMX_GPT_H_
#define PLAT_IMX_GPT_H_

#include <pb/pb.h>
#include <pb/io.h>

#define GP_TIMER_CR  0x0000
#define GP_TIMER_PR  0x0004
#define GP_TIMER_SR  0x0008
#define GP_TIMER_CNT 0x0024

int gp_timer_init(__iomem base_addr, unsigned int pr);
unsigned int gp_timer_get_tick(void);

#endif  // PLAT_IMX_GPT_H_
