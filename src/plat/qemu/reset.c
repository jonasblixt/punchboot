
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include "gcov.h"
#include <pb/pb.h>
#include <pb/plat.h>
#include <plat/qemu/semihosting.h>
#include <stdio.h>

void plat_reset(void)
{
    int rc = 0;
#ifdef CONFIG_QEMU_ENABLE_TEST_COVERAGE
    rc = gcov_store_output();

    if (rc < 0) {
        LOG_ERR("GCOV error (%i)", rc);
    }
#endif
    semihosting_sys_exit(rc);
}
