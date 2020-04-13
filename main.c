/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/timing_report.h>
#include <pb/storage.h>
#include <pb/command.h>
#include <pb/crypto.h>
#include <pb/board.h>
#include <pb/boot.h>

void pb_main(void)
{
    int rc;
    void *plat = NULL;

#ifdef CONFIG_ENABLE_WATCHDOG
    plat_wdog_init();
#endif

    /*
     * Perform really early stuff, like setup RAM and other
     * arch/platform specific tasks
     */
    pb_storage_early_init();
    rc = plat_early_init();

    if (rc != PB_OK)
        plat_reset();


    /* TODO: REMOVE */
    plat = plat_get_private();

    tr_stamp_begin(TR_BLINIT);
    tr_stamp_begin(TR_TOTAL);

    plat_console_init();

#if LOGLEVEL > 0
    printf("\n\r\n\rPB " PB_VERSION " starting\n\r");
#endif

    rc = plat_crypto_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize crypto");
        goto run_command_mode;
    }

    rc = pb_storage_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize storage");
        goto run_command_mode;
    }

    rc = plat_slc_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize SLC");
        goto run_command_mode;
    }

    rc = pb_boot_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not load state");
        goto run_command_mode;
    }

    rc = board_pre_boot(plat);

    if (rc != PB_OK)
    {
        LOG_ERR("Board pre_boot failed");
        goto run_command_mode;
    }

    if (board_force_command_mode(plat))
    {
        LOG_INFO("Forced command mode");
        goto run_command_mode;
    }

    rc = pb_boot_load_fs(NULL);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not boot");
        goto run_command_mode;
    }

    pb_boot(false);

    LOG_ERR("Boot failed");

run_command_mode:
    pb_command_run();
    plat_reset();
}
