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
#include <pb/timing_report.h>
#include <pb/command.h>
#include <pb/storage.h>
#include <pb/transport.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/boot.h>

static struct pb_console console;
static struct pb_storage storage;
static struct pb_transport transport;
static struct pb_crypto crypto;
static struct pb_command cmd;
static struct pb_command_context command_ctx;
static struct pb_boot_context boot_ctx;
extern struct bpak_keystore keystore_pb;
static uint8_t device_uuid[16] __no_bss;

static int pb_early_init(void)
{
    int rc;

    tr_init();

    rc = pb_console_init(&console);

    if (rc != PB_OK)
        return rc;

    rc = pb_storage_init(&storage);

    if (rc != PB_OK)
        return rc;

    rc = pb_transport_init(&transport, device_uuid);

    if (rc != PB_OK)
        return rc;

    rc = pb_crypto_init(&crypto);

    if (rc != PB_OK)
        return rc;

    rc = pb_command_init(&command_ctx, &transport, &storage, &crypto,
                            &keystore_pb);

    if (rc != PB_OK)
        return rc;

    rc = plat_early_init(&storage, &transport, &console, &crypto, &command_ctx,
                         &boot_ctx);

    if (rc != PB_OK)
        return rc;

    rc = pb_console_start(&console);

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

int putchar(int c)
{
    if (console.driver->ready)
    {
        console.driver->write(console.driver, (char *) &c, 1);
    }

    return c;
}

void pb_main(void)
{
    int rc;
    int recovery_timeout_ts;
    bool flag_run_command_mode = false;

    rc = pb_early_init();

    if (rc != PB_OK)
        plat_reset();

    tr_stamp_begin(TR_BLINIT);
    tr_stamp_begin(TR_TOTAL);

    LOG_INFO("\n\r\n\rPB " PB_VERSION " starting");

    rc = pb_storage_start(&storage);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize storage");
        flag_run_command_mode = true;
        goto run_command_mode;
    }

    rc = pb_crypto_start(&crypto);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not initialize crypto");
        flag_run_command_mode = true;
        goto run_command_mode;
    }

    rc = plat_get_uuid(&crypto, device_uuid);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not read device UUID");
        flag_run_command_mode = true;
        goto run_command_mode;
    }

    rc = pb_boot(&boot_ctx, NULL, false, false);

    if (rc != PB_OK)
    {
        LOG_ERR("Could not boot, starting command mode");
    }

    flag_run_command_mode = true;
run_command_mode:

    recovery_timeout_ts = plat_get_us_tick();

    if (flag_run_command_mode)
    {
        LOG_DBG("Starting transport");

        rc = pb_transport_start(&transport);

        if (rc != PB_OK)
        {
            LOG_ERR("Transport init err");
            plat_reset();
        }

        LOG_DBG("Transport init done");

        while (flag_run_command_mode)
        {
            rc = pb_transport_read(&transport, &cmd, sizeof(cmd));

            if (rc != PB_OK)
            {
                LOG_ERR("Read error %i", rc);
                continue;
            }

            rc = pb_command_parse(&command_ctx, &cmd);

            if (rc != PB_OK)
            {
                LOG_ERR("Command error %i", rc);
            }
        }
    }

    plat_reset();
}
