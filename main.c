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
#include <pb/arch.h>
#include <pb/plat.h>
#include <pb/timestamp.h>
#include <pb/storage.h>
#include <pb/command.h>
#include <pb/crypto.h>
#include <pb/boot.h>

static struct pb_timestamp ts_plat_early = TIMESTAMP("Platform early");
static struct pb_timestamp ts_crypto = TIMESTAMP("Crypto");
static struct pb_timestamp ts_storage = TIMESTAMP("Storage");
static struct pb_timestamp ts_slc = TIMESTAMP("SLC");
static struct pb_timestamp ts_boot_init = TIMESTAMP("Boot init");
static struct pb_timestamp ts_total = TIMESTAMP("Total");

void pb_main(void)
{
    int rc;

    arch_init();
    timestamp_init();

    /*
     * Perform really early stuff, like setup RAM and other
     * arch/platform specific tasks
     */
    timestamp_begin(&ts_total);
    timestamp_begin(&ts_plat_early);
    pb_storage_early_init();
    rc = plat_early_init();

    if (rc != PB_OK)
        plat_reset();

    timestamp_end(&ts_plat_early);

    plat_console_init();

#ifdef CONFIG_ENABLE_WATCHDOG
    plat_wdog_init();
#endif

#if LOGLEVEL > 0
    printf("\n\r\n\rPB " PB_VERSION " starting\n\r");
#endif

#ifdef CONFIG_ENABLE_WATCHDOG
    plat_wdog_init();
#endif

    timestamp_begin(&ts_crypto);
    rc = plat_crypto_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize crypto");
        goto run_command_mode;
    }

    timestamp_end(&ts_crypto);

    timestamp_begin(&ts_storage);
    rc = pb_storage_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize storage");
        goto run_command_mode;
    }

    timestamp_end(&ts_storage);

    timestamp_begin(&ts_slc);
    rc = plat_slc_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize SLC");
        goto run_command_mode;
    }

    timestamp_end(&ts_slc);

    timestamp_begin(&ts_boot_init);
    rc = pb_boot_init();

    if (rc != PB_OK)
    {
        LOG_ERR("Could not load boot state");
        goto run_command_mode;
    }

    timestamp_end(&ts_boot_init);
    if (plat_force_command_mode())
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

    pb_boot(&ts_total, false, false);

    LOG_INFO("Boot stopped, entering command mode");

run_command_mode:
    pb_command_run();
    plat_reset();
}
