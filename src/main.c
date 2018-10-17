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
#include <pb_image.h>

/*
static void print_bootmsg(uint32_t param1, int32_t param2, char bp) {
    tfp_printf("\n\rPB: " VERSION ", %lu ms, %lu - %li %c\n\r", plat_get_ms_tick(), \
                    param1, param2, bp);

}
*/

/*
static void print_board_uuid(void) {
    uint8_t board_uuid[16];

    board_get_uuid(board_uuid);

    tfp_printf ("Board UUID: ");
    for (int i = 0; i < 16; i++) {
        tfp_printf ("%2.2X", board_uuid[i]);
        if ((i == 3) || (i == 6) || (i == 8)) 
            tfp_printf ("-");
    }
    tfp_printf("\n\r");


}*/

void pb_nsec_main(void) 
{
}

void pb_main(void) 
{
    uint32_t err = 0;
    uint32_t boot_count = 0;
    uint32_t boot_part = 0;
    uint32_t boot_lba_offset = 0;
    bool flag_run_recovery = false;

    if (board_init() == PB_ERR) 
    {
        LOG_ERR ("Board init failed...");
        plat_reset();
    }
 
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

    if (flag_run_recovery)
        goto run_recovery;

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

    err = pb_image_load_from_fs(boot_lba_offset);

    if (err != PB_OK)
    {
        LOG_ERR("Unable to load image, starting recovery");
        flag_run_recovery = true;
    }

    err = pb_verify_image(pb_get_image(), 0);

    if (err == PB_OK)
    {
        LOG_INFO("Image verified, booting...");

        config_get_uint32_t(PB_CONFIG_BOOT_COUNT, &boot_count);
        boot_count = boot_count + 1;
        config_set_uint32_t(PB_CONFIG_BOOT_COUNT, boot_count);
        config_commit();

        struct pb_component_hdr *tee = 
                pb_image_get_component(pb_get_image(), PB_IMAGE_COMPTYPE_TEE);

        struct pb_component_hdr *vmm = 
                pb_image_get_component(pb_get_image(), PB_IMAGE_COMPTYPE_VMM);

        LOG_INFO(" VMM %lX, TEE %lX", vmm->load_addr_low, tee->load_addr_low);
        asm volatile("mov lr, %0" "\n\r"
                     "mov pc, %1" "\n\r"
                        :
                        : "r" (vmm->load_addr_low), "r" (tee->load_addr_low));
    }

run_recovery:

    if (flag_run_recovery)
    {
        usb_init();
        recovery_initialize();

        while (flag_run_recovery)
            usb_task();
    } 
    
    plat_reset();
}
