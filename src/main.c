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
#include <crypto.h>
#include <boot.h>
#include <timing_report.h>
#include <state.h>


static __no_bss __a4k struct pb_pbi pbi;
static __no_bss __a4k struct gpt gpt;
static char hash_buffer[PB_HASH_BUF_SZ];

void pb_main(void)
{
    uint32_t err = 0;
    uint32_t active_system;
    uint32_t recovery_timeout_ts;
    bool flag_run_recovery = false;
    struct gpt_part_hdr *part_system_a, *part_system_b;
    struct gpt_part_hdr *part;

    tr_init();

    if (plat_early_init() != PB_OK)
        plat_reset();

    tr_stamp_begin(TR_BLINIT);
    tr_stamp_begin(TR_TOTAL);

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
        part = part_system_a;
        active_system = SYSTEM_A;
    }
    else if (gpt_part_is_bootable(part_system_b))
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

    uint32_t count = gpt_pb_attr_counter(part);

    /* Newly upgraded system? */
    if (!gpt_pb_attr_ok(part) && (count> 0))
    {
        /* Decrement boot counter */
        count--;
        gpt_pb_attr_clrbits(part, 0x0f);
        gpt_pb_attr_setbits(part, count);

        gpt_write_tbl(&gpt);
        LOG_DBG("Current boot counter: %u", count);
    }
    else if (!gpt_pb_attr_ok(part)) /* Boot counter expired, rollback to other part */
    {
        LOG_ERR("System is not bootable, performing rollback");
        /* Indicate that this system failed by setting
         * the rollback bit
         * */
        gpt_pb_attr_setbits(part, PB_GPT_ATTR_ROLLBACK);
        gpt_part_set_bootable(part, false);

        if (active_system == SYSTEM_A)
        {
            part = part_system_b;
            active_system = SYSTEM_B;
        }
        else
        {
            part = part_system_a;
            active_system = SYSTEM_A;
        }

        /* In the unlikely event that the other system is also broken,
         * start recovery mode
         * */

        if (!gpt_pb_attr_ok(part))
        {
            LOG_ERR("Other system is also broken, starting recovery");
            flag_run_recovery = true;
            goto run_recovery;
        }

        gpt_part_set_bootable(part, true);
        gpt_write_tbl(&gpt);
    }

    err = pb_image_load_from_fs(part->first_lba, &pbi, hash_buffer);

    if (err != PB_OK)
    {
        LOG_ERR("Unable to load image");
        flag_run_recovery = true;
        goto run_recovery;
    }

    err = pb_image_verify(&pbi, (const char*) hash_buffer);

    if (err == PB_OK)
    {
        LOG_INFO("Image verified, booting...");
        pb_boot(&pbi, active_system,false);
    }
    else
    {
        LOG_ERR("Could not boot image, entering recovery mode...");
        flag_run_recovery = true;
    }

run_recovery:

    recovery_timeout_ts = plat_get_us_tick();

    if (flag_run_recovery)
    {
        LOG_INFO("Initializing recovery mode");
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

    plat_reset();
}
