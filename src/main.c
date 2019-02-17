/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <pb.h>
#include <stdio.h>
#include <plat.h>
#include <recovery.h>
#include <gpt.h>
#include <io.h>
#include <image.h>
#include <keys.h>
#include <boot.h>
#include <timing_report.h>


static __no_bss __a4k struct pb_pbi pbi;
static __no_bss __a4k struct gpt gpt;

void pb_main(void) 
{
    uint32_t err = 0;
    uint32_t active_system;
    bool flag_run_recovery = false;
    struct gpt_part_hdr *part_system_a, *part_system_b;

    tr_init();

    if (plat_early_init() != PB_OK)
        plat_reset();

    tr_stamp_begin(TR_BLINIT);

    LOG_INFO ("PB: " VERSION " starting...");

    if (gpt_init(&gpt) != PB_OK)
        flag_run_recovery = true;



    if (plat_force_recovery())
        flag_run_recovery = true;

    tr_stamp_end(TR_BLINIT);

    if (gpt_get_part_by_uuid(&gpt, PB_PARTUUID_SYSTEM_A, &part_system_a) != PB_OK)
        flag_run_recovery = true;

    if (gpt_get_part_by_uuid(&gpt, PB_PARTUUID_SYSTEM_B, &part_system_b) != PB_OK)
        flag_run_recovery = true;

    if (flag_run_recovery)
        goto run_recovery;

    if (gpt_part_is_bootable(part_system_a))
    {
        LOG_INFO("Loading System A");
        err = pb_image_load_from_fs((part_system_a->first_lba), &pbi);
        active_system = SYSTEM_A;
    }
    else if (gpt_part_is_bootable(part_system_b))
    {
        LOG_INFO("Loading System B");
        err = pb_image_load_from_fs(part_system_b->first_lba, &pbi);
        active_system = SYSTEM_B;
    }
    else
    {
        LOG_INFO("No bootable system found");
        flag_run_recovery = true;

        goto run_recovery;
    }
    
    if (err != PB_OK)
    {
        LOG_ERR("Unable to load image");
        flag_run_recovery = true;
        LOG_DBG("array crc %x", gpt.primary.hdr.part_array_crc);
        goto run_recovery;
    }

    err = pb_image_verify(&pbi);

    if (err == PB_OK)
    {
        LOG_INFO("Image verified, booting...");
        pb_boot(&pbi, active_system);
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

        recovery_initialize(&gpt);

        while (flag_run_recovery)
        {
            usb_task();
            plat_wdog_kick();
        }
    } 
    
    plat_reset();
}
