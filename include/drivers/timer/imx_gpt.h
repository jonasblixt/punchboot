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

int imx_gpt_init(uintptr_t base, unsigned int input_clock_Hz);
unsigned int imx_gpt_get_tick(void);

#endif  // PLAT_IMX_GPT_H_
