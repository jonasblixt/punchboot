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


#define DEBUG

void pb_main(void) {
    const char pelle[] = "Arne";
    unsigned char hash[32];
    hash_state md;

    board_critical_init();

#ifdef DEBUG
    board_uart_init();
    init_printf(NULL,soc_uart_putc);
    tfp_printf("\n\rPB: " VERSION "\n\r");
#endif


    board_emmc_init();


 /*   tfp_printf("memcpy works:\n\r");
    memcpy(hash, pelle,4);
    memset(hash, 'A', 32);
    for (int i = 0; i < 32; i++) {
        tfp_printf("%i %c\n\r",i, hash[i]);
    }
*/

    /*
    sha256_init(&md);
    sha256_process(&md, (const unsigned char*) pelle, 4);
    sha256_done(&md, hash);


    tfp_printf ("SHA256: ");

    for (int i = 0; i < 32; i++)
        tfp_printf ("%.2x",hash[i]);
    tfp_printf("\n\r");
    */

#ifndef DEBUG
    board_uart_init();
    init_printf(NULL,soc_uart_putc);
    tfp_printf("\n\rPB: " VERSION "\n\r");
#endif


    recovery();


}
