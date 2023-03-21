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
#include <pb/command.h>
#include <boot/boot.h>

void pb_main(void)
{
    int rc;

    arch_init();

    rc = plat_init();

    if (rc == PB_OK) {
        rc = boot(NULL);  /* Normally we would not return from this function */
    }

    printf("Boot aborted (%i), entering command mode\n\r", rc);
    pb_command_run();
    plat_reset();
}
