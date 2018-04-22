/* Punch BOOT 
 *
 *
 *
 * */

#include <board.h>
#include <regs.h>
#include <io.h>
#include <uart.h>
#include <types.h>
#include <tinyprintf.h>
#include <tomcrypt.h>
#include <recovery.h>
#include <gpt.h>
#include <plat.h>
#include <emmc.h>

void pb_main(void) {
    board_critical_init();
    board_uart_init();
    board_emmc_init();
    gpt_init();

   
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

    tfp_printf("\n\rPB: " VERSION ", init took  %i ms\n\r", plat_get_ms_tick());
    recovery();

}
