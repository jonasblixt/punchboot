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

void pb_main(void) 
{
    struct pb_pbi *pbi = NULL;
    uint32_t err = 0;
    uint32_t boot_count = 0;
    uint32_t boot_part = 0;
    uint32_t boot_lba_offset = 0;
    uint32_t ts_init;
    uint32_t ts0, ts1, ts11, ts2, ts3, ts4;

    bool flag_run_recovery = false;

    plat_wdog_init();

    if (board_init() == PB_ERR) 
    {
        LOG_ERR ("Board init failed...");
        plat_reset();
    }
    //pb_writel(0x4000, 0x020A0000);
    ts_init = plat_get_us_tick();

    LOG_INFO ("PB: " VERSION " starting...");

    if (gpt_init() != PB_OK)
        flag_run_recovery = true;
    
    ts0 = plat_get_us_tick();

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

    ts1 = plat_get_us_tick();

    switch (boot_part)
    {
        case 0xAA:
            LOG_INFO ("Loading from system A");
            err = gpt_get_part_by_uuid(part_type_system_a, &boot_lba_offset);
            if (err != PB_OK)
                flag_run_recovery = true;
        break;
        case 0xBB:
            LOG_INFO ("Loading from system B");
            err = gpt_get_part_by_uuid(part_type_system_b, &boot_lba_offset);
            if (err != PB_OK)
                flag_run_recovery = true;
        break;
        default:
            LOG_ERR("Invalid boot partition %lx", boot_part);
            flag_run_recovery = true;
    }
    ts11 = plat_get_us_tick();

    err = pb_image_load_from_fs(boot_lba_offset, &pbi);
    
    ts2 = plat_get_us_tick();

    if (err != PB_OK)
    {
        LOG_ERR("Unable to load image, starting recovery");
        flag_run_recovery = true;
    }

    err = pb_image_verify(pbi, PB_KEY_DEV);
    
    ts3 = plat_get_us_tick();

    if (err == PB_OK)
    {
        LOG_INFO("Image verified, booting...");

        config_get_uint32_t(PB_CONFIG_BOOT_COUNT, &boot_count);
        boot_count = boot_count + 1;
        config_set_uint32_t(PB_CONFIG_BOOT_COUNT, boot_count);
        config_commit();

        ts4 = plat_get_us_tick();
        pb_writel(0, 0x020A0000);
        asm("isb");
        
        tfp_printf ("Total: %lu us\n\r",ts4-ts_init);
        tfp_printf ("gpt: %lu us\n\r", ts0-ts_init);
        tfp_printf ("cfg: %lu us\n\r", ts1-ts0);
        tfp_printf ("getpart: %lu us\n\r", ts11-ts1);
        tfp_printf ("load %lu us\n\r", ts2-ts11);
        tfp_printf ("vrfy: %lu us\n\r", ts3-ts2);
        tfp_printf ("bc: %lu us\n\r", ts4-ts3);
        tfp_printf ("ts_init: %lu us\n\r",ts_init);
        
        board_boot(pbi);
    }

run_recovery:

    if (flag_run_recovery)
    {
        usb_init();
        recovery_initialize();

        while (flag_run_recovery)
        {
            usb_task();
            plat_wdog_kick();
        }
    } 
    
    plat_reset();
}
