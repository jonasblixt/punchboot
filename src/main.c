/* Punch BOOT 
 *
 *
 * Notes:
 *   - Do not use 64bit division or 64bit variables with tinyprintf
 *       '__aeabi_uldivmod' calls will cause an endless loop. See
 *       start-up assembly code.
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
    init_printf(NULL,soc_uart_putc);
    board_emmc_init();
    gpt_init();

    int t1 = plat_get_ms_tick();
    usdhc_emmc_xfer_blocks(0x1000, (u8 *) 0x87800000,
                            2048, 0, 0);
    int t2 = plat_get_ms_tick();

   
    /* 
     * POR: 28 ms (Without HAB)
     * Init: 7ms
     * Read: 13ms
     * tomcrypt_sha256_400kByte: 431ms
     * tomcrypt_verify_rsa4096: 567ms
     *
     * caam_sha256_400kByte: ??? ms
     * caam_verify_rsa4096: ??? ms
     *
     * */

    tfp_printf("\n\rPB: " VERSION ", init took  %i ms\n\r", t1);
    tfp_printf ("Reading 1 Mbyte took %i ms (%i MB/s)\n\r",t2-t1, 1024/(t2-t1));
    recovery();


}
