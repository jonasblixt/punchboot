
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/qemu/semihosting.h>
#include "pl061.h"
#include "gcov.h"

void plat_reset(void)
{
#ifdef CONFIG_QEMU_ENABLE_TEST_COVERAGE
    gcov_final();
#endif
    semihosting_sys_exit(0);
}

