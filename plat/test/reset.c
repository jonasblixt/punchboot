
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb.h>
#include <plat.h>
#include <plat/test/semihosting.h>
#include "pl061.h"
#include "gcov.h"

void plat_reset(void)
{
    gcov_final();
    semihosting_sys_exit(0);
}

