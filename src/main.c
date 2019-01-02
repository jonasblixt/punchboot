/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <recovery.h>
#include <config.h>
#include <gpt.h>
#include <board_config.h>
#include <io.h>
#include <image.h>
#include <keys.h>
#include <boot.h>

void pb_main(void) 
{
    uint32_t err = 0;
    uint32_t boot_part = 0;
    bool flag_run_recovery = false;
    struct pb_pbi *pbi = NULL;

    plat_wdog_init();

    if (plat_early_init() != PB_OK)
        plat_reset();

    LOG_INFO ("PB: " VERSION " starting...");

    if (gpt_init() != PB_OK)
        flag_run_recovery = true;

    if (config_init() != PB_OK)
        flag_run_recovery = true;

    if (board_force_recovery())
        flag_run_recovery = true;

    err = config_get_uint32_t(PB_CONFIG_BOOT, &boot_part);

    if (err != PB_OK)
        flag_run_recovery = true;

    if (!flag_run_recovery)
    {
        uint32_t flag;
        config_get_uint32_t(PB_CONFIG_FORCE_RECOVERY, &flag);

        if (flag > 0)
            flag_run_recovery = true;
    }

    if (flag_run_recovery)
        goto run_recovery;


    if (pb_boot_load_part((uint8_t) boot_part, &pbi) == PB_OK)
    {
        LOG_INFO("Image on part %2.2X verified, booting...", (uint8_t) boot_part);
        pb_boot_image(pbi);
    } else {
        LOG_ERR("Could not boot image on part %2.2X, entering recovery mode...",
                        (uint8_t) boot_part);
        flag_run_recovery = true;
    }

run_recovery:

    if (flag_run_recovery)
    {
        err = usb_init();
		if (err != PB_OK)
		{
			LOG_ERR("Could not initialize USB");
			plat_reset();
		}
        recovery_initialize();

        while (flag_run_recovery)
        {
            usb_task();
            plat_wdog_kick();
        }
    } 
    
    plat_reset();
}
