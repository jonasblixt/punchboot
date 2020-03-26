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
#include <pb/gpt.h>
#include <pb/io.h>
#include <pb/image.h>
#include <pb/crypto.h>
#include <pb/boot.h>
#include <pb/timing_report.h>
#include <pb/config.h>
#include <bpak/bpak.h>
#include <pb/command.h>
#include <pb/storage.h>
#include <pb/transport.h>

static struct pb_storage storage;
static struct pb_transport transport;

void pb_main(void)
{
    int rc;
    int recovery_timeout_ts;
    bool flag_run_recovery = false;

    tr_init();

    rc = pb_storage_init(&storage);

    if (rc != PB_OK)
        plat_reset();

    rc = pb_transport_init(&transport);

    if (rc != PB_OK)
        plat_reset();

    if (plat_early_init(&storage, &transport) != PB_OK)
        plat_reset();

    tr_stamp_begin(TR_BLINIT);
    tr_stamp_begin(TR_TOTAL);

    LOG_INFO("PB " PB_VERSION " starting");

    rc = pb_storage_start(&storage);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize storage");
        flag_run_recovery = true;
        //goto run_recovery;
    }

    LOG_DBG("Starting transport");

    rc = pb_transport_start(&transport);

    if (rc != PB_OK)
    {
        LOG_ERR("Transport init err");
    }

    LOG_DBG("Transport init done");

    struct pb_command cmd;
    while (1)
    {
        plat_wdog_kick();

        rc = pb_transport_read(&transport, &cmd, sizeof(cmd));

        LOG_DBG("%i cmd %i", rc, cmd.command);
    }


#ifdef __NOPE

    if (plat_force_recovery())
        flag_run_recovery = true;

    tr_stamp_end(TR_BLINIT);

    /* Load system partition headers */

    if (gpt_get_part_by_uuid(PB_PARTUUID_SYSTEM_A, &part_system_a) != PB_OK)
    {
        LOG_ERR("Could not find system A");
        flag_run_recovery = true;
    }

    if (gpt_get_part_by_uuid(PB_PARTUUID_SYSTEM_B, &part_system_b) != PB_OK)
    {
        LOG_ERR("Could not find system B");
        flag_run_recovery = true;
    }

    if (config_init() != PB_OK)
        flag_run_recovery = true;

    if (flag_run_recovery)
        goto run_recovery;

    if (config_system_enabled(SYSTEM_A))
    {
        LOG_INFO("Loading System A");
        part = part_system_a;
        active_system = SYSTEM_A;
    }
    else if (config_system_enabled(SYSTEM_B))
    {
        LOG_INFO("Loading System B");
        part = part_system_b;
        active_system = SYSTEM_B;
    }
    else
    {
        LOG_INFO("No bootable system found");
        flag_run_recovery = true;

        goto run_recovery;
    }

    uint32_t count = config_get_remaining_boot_attempts();

    LOG_DBG("Current boot counter: %u", count);

    /* Newly upgraded system? */
    if (!config_system_verified(active_system) && (count> 0))
    {
        /* Decrement boot counter */
        config_decrement_boot_attempt();
        config_commit();
    }
    else if (!config_system_verified(active_system))
    {
        /* Boot counter expired, rollback to other part */
        LOG_ERR("System is not bootable, performing rollback");
        /* Indicate that this system failed by setting
         * the rollback bit
         * */


        if (active_system == SYSTEM_A)
        {
            part = part_system_b;
            active_system = SYSTEM_B;
            config_system_enable(SYSTEM_B);
            config_set_boot_error(PB_CONFIG_ERROR_A_ROLLBACK);
        }
        else
        {
            part = part_system_a;
            active_system = SYSTEM_A;
            config_system_enable(SYSTEM_A);
            config_set_boot_error(PB_CONFIG_ERROR_B_ROLLBACK);
        }

        config_commit();

        /* In the unlikely event that the other system is also broken,
         * start recovery mode
         * */

        if (!config_system_verified(active_system))
        {
            LOG_ERR("Other system is also broken, starting recovery");
            flag_run_recovery = true;
            goto run_recovery;
        }
    }

    err = pb_image_load_from_fs(part->first_lba,
                                part->last_lba,
                                &h,
                                hash_buffer);

    if (err == PB_OK)
    {
        LOG_INFO("Image verified, booting...");
        pb_boot(&h, active_system, false);
    }
    else
    {
        LOG_ERR("Could not boot image, entering recovery mode...");
        flag_run_recovery = true;
    }

flag_run_recovery = true;
run_recovery:

    recovery_timeout_ts = plat_get_us_tick();

    if (flag_run_recovery)
    {
        LOG_INFO("Initializing command mode");

        rc = command_initialize(&storage);

        if (rc != PB_OK)
        {
            LOG_ERR("Could not initialize recovery mode");
            plat_reset();
        }
        while (flag_run_recovery)
        {
            usb_task();
            plat_wdog_kick();

            uint32_t recovery_timeout_counter = plat_get_us_tick() -
                                                        recovery_timeout_ts;

            if (!usb_has_enumerated() &&
                          (recovery_timeout_counter > PB_RECOVERY_TIMEOUT_US))
            {
                LOG_INFO("Recovery timeout, rebooting...");
                plat_reset();
            }
        }
    }

#endif
    plat_reset();
}
