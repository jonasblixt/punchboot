/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <pb/pb.h>
#include <pb/arch.h>
#include <pb/plat.h>
#include <pb/command.h>
#include <pb/boot.h>

void pb_main(void)
{
    int rc;

    arch_init();

    rc = platform_init();

    if (rc != 0)
        goto run_command_mode;

    rc = pb_boot_init();

    if (rc != PB_OK) {
        LOG_ERR("Could not load boot state");
        goto run_command_mode;
    }

    if (plat_force_command_mode()) {
        LOG_INFO("Forced command mode");
        goto run_command_mode;
    }

    rc = pb_boot_load(PB_BOOT_SOURCE_BLOCK_DEV, NULL);

    if (rc != PB_OK) {
        LOG_ERR("Could not boot");
        goto run_command_mode;
    }

    pb_boot(PB_BOOT_MODE_NORMAL, false);

    LOG_INFO("Boot stopped, entering command mode");

run_command_mode:
    pb_command_run();
    plat_reset();
}
