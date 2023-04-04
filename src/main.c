/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/arch.h>
#include <pb/plat.h>
#include <pb/timestamp.h>
#include <pb/cm.h>
#include <boot/boot.h>
#include <pb/self_test.h>

void main(void)
{
    int rc;

    arch_init();

    rc = plat_init();

    if (rc != PB_OK)
        // TODO: Here we should panic. It's possible that:
        // 1. We don't have console
        // 2. WDT is not enabled
        // 3. Systick is not working
        //
        // plat_reset?
        goto enter_command_mode;

    rc = plat_board_init();

    if (rc != PB_OK)
        goto enter_command_mode;

#ifdef CONFIG_SELF_TEST
    self_test();
#endif

    rc = boot(NULL);  /* Normally we would not return from this function */

enter_command_mode:
    printf("Boot aborted (%i), entering command mode\n\r", rc);
    ts_print();
#ifdef CONFIG_CM
    cm_run();
#endif
    plat_reset();
}
