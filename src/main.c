/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <boot/boot.h>
#include <pb/arch.h>
#include <pb/cm.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/ramlog.h>
#include <stdio.h>
#include <stdlib.h>

void main(void)
{
    int rc;

#ifdef CONFIG_RAMLOG
    /* First, so that prints from before console_init() are kept and written
     * to the UART once it's set up */
    ramlog_init();
#endif
    arch_init();

    rc = plat_init();

    if (rc != PB_OK)
        goto enter_command_mode;

    rc = plat_board_init();

    if (rc != PB_OK)
        goto enter_command_mode;

    rc = boot(NULL); /* Normally we would not return from this function */

enter_command_mode:
    printf("Boot aborted (%i), entering command mode\n\r", rc);
#ifdef CONFIG_CM
    cm_run();
#endif
    plat_reset();
}

#ifndef CONFIG_ARCH_HOST
/* The host build links against the system libc, which provides exit() */
void exit(int reason)
{
    LOG_ERR("reason = %i", reason);
    plat_reset();
}
#endif
