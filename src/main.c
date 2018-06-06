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
#include <boot.h>
#include <board_config.h>

#undef MAIN_DEBUG

static void print_bootmsg(uint32_t param1, int32_t param2, char bp) {
    tfp_printf("\n\rPB: " VERSION ", %lu ms, %lu - %li %c\n\r", plat_get_ms_tick(), \
                    param1, param2, bp);

}

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


}

void pb_main(void) {

    bool flag_corrupt_config = false;
    bool flag_corrupt_gpt = false;
    bool flag_force_recovery = false;


    if (board_init() == PB_ERR) {
        tfp_printf ("Board init failed...\n\r");
        plat_reset();
    }

    if (gpt_init() == PB_ERR) {
        flag_corrupt_gpt = true;
        flag_force_recovery = true;
    }

    if (config_init() == PB_ERR) {
        flag_corrupt_config = true;
        flag_force_recovery = true;
    }
  
    if (board_force_recovery() || flag_force_recovery) {
        print_bootmsg(0, 0, '?');
        print_board_uuid();
        if (flag_corrupt_gpt)
            tfp_printf ("GPT is missing or corrupt: ");
        else if (flag_corrupt_config)
            tfp_printf ("Config is missing or corrupt: ");
        tfp_printf ("Forcing recovery mode\n\r");
        recovery();
        tfp_printf ("recovery() returned...\n\r");
        plat_reset();
    }


   /* 
     * POR: 28 ms (Without HAB)
     * Init: 7ms
     * Read: 13ms
     * tomcrypt_sha256_400kByte: 431ms
     * tomcrypt_verify_rsa4096: 567ms
     *
     * caam_sha256_400kByte: 4 ms !!  (130 MByte/s)
     * caam_verify_rsa4096: 5 ms
     *
     *
     *
     *  Total boot time from reset de-assertion (Voltage stable), without HAB
     *    POR  28 ms
     *    Init 7ms
     *    Read 13ms
     *    SHA  4ms
     *    RSA  5ms
     *  ------------------
     *    + 57 ms
     *
     * */

    boot_inc_boot_count();

    uint32_t boot_part = 0;
    uint32_t boot_count = 0;
    char boot_part_c = '?';

    config_get_uint32_t(PB_CONFIG_BOOT, &boot_part);
    config_get_uint32_t(PB_CONFIG_BOOT_COUNT, &boot_count);

    if (boot_part == PB_BOOT_A)
        boot_part_c = 'A';
    if (boot_part == PB_BOOT_B)
        boot_part_c = 'B';

    if ( (boot_part == PB_BOOT_A) || (boot_part == PB_BOOT_B)) {
        if (boot_load(boot_part) == PB_OK) {
            print_bootmsg(boot_count, 
                            boot_fail_count(boot_part), boot_part_c);
            boot();
        } else if (boot_fail_count(boot_part) > PB_MAX_FAIL_BOOT_COUNT){
            print_bootmsg(boot_count, 
                            boot_fail_count(boot_part), boot_part_c);
 
            tfp_printf("System %c has failed too many times, Entering recovery mode\n\r",boot_part_c);
            recovery();
        } else {
            boot_inc_fail_count(boot_part);
            plat_reset();
        }
    } else {
        print_bootmsg(boot_count,-1, '?');
        tfp_printf ("Could not figure out which system to boot\n\r");
        recovery();
    }

    plat_reset(); /* Should not reach this */
}
