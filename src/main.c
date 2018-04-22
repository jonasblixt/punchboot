/* Punch BOOT 
 *
 *
 *
 * */

#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <recovery.h>
#include <config.h>
#include <boot.h>
#include <board_config.h>

static void print_bootmsg(s32 param1, s32 param2) {
    tfp_printf("\n\rPB: " VERSION ", %i ms, %i - %i\n\r", plat_get_ms_tick(), 
                    param1, param2);
}

void pb_main(void) {
    u8 flag_corrupt_config = false;

    if (board_init() == PB_ERR)
        plat_reset();

    if (gpt_init() == PB_ERR) {
        flag_corrupt_config = true;
    }

    if (config_init() == PB_ERR) {
        flag_corrupt_config = true;
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

    if (board_force_recovery() || flag_corrupt_config) {
        print_bootmsg(-1,0);
        if (flag_corrupt_config)
            tfp_printf ("Config/GPT is missing or corrupt: ");
        tfp_printf ("Forcing recovery mode\n\r");
        recovery();
    }

    if (config_get_byte(PB_CONFIG_BOOT) == PB_BOOT_A) {

        if (boot_load(PB_BOOT_A) == PB_OK) {
            print_bootmsg(boot_boot_count(PB_BOOT_A), 
                            boot_fail_count(PB_BOOT_A));
            boot();
        } else if (boot_fail_count(PB_BOOT_A) > PB_MAX_FAIL_BOOT_COUNT){
            tfp_printf("System A has failed too many times, Entering recovery mode\n\r");
            recovery();
        } else {
            boot_inc_fail_count(PB_BOOT_A);
            plat_reset();
        }

    } else if (config_get_byte(PB_CONFIG_BOOT) == PB_BOOT_B) {

        if (boot_load(PB_BOOT_B) == PB_OK) {
            print_bootmsg(boot_boot_count(PB_BOOT_B), 
                            boot_fail_count(PB_BOOT_B));
            boot();
        } else if (boot_fail_count(PB_BOOT_B) > PB_MAX_FAIL_BOOT_COUNT){
            print_bootmsg(boot_boot_count(PB_BOOT_B), 
                            boot_fail_count(PB_BOOT_B));
            tfp_printf("System A has failed too many times, Entering recovery mode\n\r");
            recovery();
        } else {
            boot_inc_fail_count(PB_BOOT_B);
            plat_reset();
        }

    } else {
        print_bootmsg(-1,-1);
        tfp_printf ("Could not figure out which system to boot\n\r");
        recovery();
    }

    plat_reset(); /* Should not reach this */
}
