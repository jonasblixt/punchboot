/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#include <stdio.h>
#include <board.h>
#include <plat.h>
#include <recovery.h>
#include <gpt.h>
#include <board/config.h>
#include <io.h>
#include <image.h>
#include <keys.h>
#include <boot.h>
#include <timing_report.h>


void pb_main(void) 
{
    uint32_t err = 0;
    bool flag_run_recovery = false;
    uint32_t system_index = 0;
    struct pb_pbi *pbi = NULL;
    struct gpt_part_hdr *boot_part_a, *boot_part_b;
    uint64_t *boot_part_attr_a, *boot_part_attr_b;
    tr_init();

    if (plat_early_init() != PB_OK)
        plat_reset();

    tr_stamp_begin(TR_BLINIT);

    LOG_INFO ("PB: " VERSION " starting...");

    if (gpt_init() != PB_OK)
        flag_run_recovery = true;

    if (board_force_recovery())
        flag_run_recovery = true;

    tr_stamp_end(TR_BLINIT);

    if (gpt_get_part_by_uuid(part_type_system_a, &boot_part_a) != PB_OK)
        flag_run_recovery = true;

    if (gpt_get_part_by_uuid(part_type_system_b, &boot_part_b) != PB_OK)
        flag_run_recovery = true;

    if (flag_run_recovery)
        goto run_recovery;

    boot_part_attr_a = (uint64_t *) boot_part_a->attr;
    boot_part_attr_b = (uint64_t *) boot_part_b->attr;

    if (((*boot_part_attr_a) & GPT_ATTR_BOOTABLE) == GPT_ATTR_BOOTABLE)
    {
        LOG_INFO("Loading System A");
        system_index = 1;
        err = pb_image_load_from_fs((boot_part_a->first_lba), &pbi);
    } 
    else if (((*boot_part_attr_b) & GPT_ATTR_BOOTABLE) == GPT_ATTR_BOOTABLE) 
    {
        LOG_INFO("Loading System B");
        system_index = 2;
        err = pb_image_load_from_fs(boot_part_b->first_lba, &pbi);
    } else {
        LOG_INFO("No bootable system found");
        flag_run_recovery = true;
        goto run_recovery;
    }
    
    if (err != PB_OK)
    {
        LOG_ERR("Unable to load image");
        flag_run_recovery = true;
        goto run_recovery;
    }

    err = pb_image_verify(pbi);

    if (err == PB_OK)
    {
        LOG_INFO("Image verified, booting...");
#ifdef PB_BOOT_LINUX
                pb_boot_linux_with_dt(pbi, system_index);
#elif PB_BOOT_TEST
                plat_reset();
#endif
    } else {
        LOG_ERR("Could not boot image, entering recovery mode...");
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
