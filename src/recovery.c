#include <recovery.h>
#include <board.h>
#include <tinyprintf.h>


void recovery(void) {

    tfp_printf ("\n\r*** RECOVERY MODE ***\n\r");


    board_usb_init();

}
