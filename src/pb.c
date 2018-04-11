#include <stdio.h>
#include "pb.h"
#include "pb_types.h"
#include "pb_io.h"
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

int _write(int file, char *ptr, int len)
{
    int sz = len;
    do {
        uart_putc(*ptr++);
        if (*ptr == 0)
            break;
    } while(--sz);
    return len;
}

void punch_boot_init(void) {
    unsigned char dummy[32];
    char pelle[20] = "Arne";
    unsigned char hash[32];
    hash_state md;
    
    printf("\n\rPB: " VERSION "\n\r");

    sha256_init(&md);
    sha256_process(&md, (const unsigned char*) pelle, strlen(pelle));
    sha256_done(&md, hash);


    printf ("SHA256: \n\r");

    for (int i = 0; i < 32; i++)
        printf ("%.2x",hash[i]);
    printf("\n\r");
    


    while(1)
        asm("nop");

}
