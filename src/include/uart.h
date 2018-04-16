#ifndef __UART_H_
#define __UART_H_

#include <types.h>

void soc_uart_putc(void *, char c);
void soc_uart_init(u32 uart_base);


#endif
