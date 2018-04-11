#include <stdio.h>
#include "pb.h"
#include "pb_types.h"
#include "pb_io.h"
#include "mini-printf.h"

#include <tomcrypt.h>


static void uart_putc(unsigned char c) {
    volatile u32 usr2;

    //while (!(pb_readl(UART_PHYS + USR2) & (USR2_TXFE)));
    
    for (;;) {
        usr2 = pb_readl((void *)UART_PHYS + USR2);

        if (usr2 & (1<<3))
            break;
    }
    pb_writel(c,(void *) UART_PHYS + UTXD);

}

void uart_puts(unsigned char *s)
{
    unsigned char *s_ptr = s;
    
    do {
        uart_putc(*s_ptr);
    } while (*s_ptr++);

}

void punch_boot_init(void) {
    unsigned char buf[256];

    //register_hash(&sha256_desc);

    mini_snprintf(buf,sizeof(buf), "\n\rHello World from Punch Boot\n\r");
    uart_puts(buf);   

    sprintf(buf,"Kalle Anka\n\r");
    uart_puts(buf);

    while(1)
        asm("nop");

}
