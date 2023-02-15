/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#ifndef PLAT_IMX_IMX_UART_H_
#define PLAT_IMX_IMX_UART_H_

#include <pb/pb.h>


/* Register definitions */
#define URXD  0x0  /* Receiver Register */
#define UTXD  0x40 /* Transmitter Register */
#define UCR1  0x80 /* Control Register 1 */
#define UCR2  0x84 /* Control Register 2 */
#define UCR3  0x88 /* Control Register 3 */
#define UCR4  0x8c /* Control Register 4 */
#define UFCR  0x90 /* FIFO Control Register */
#define USR1  0x94 /* Status Register 1 */
#define USR2  0x98 /* Status Register 2 */
#define UESC  0x9c /* Escape Character Register */
#define UTIM  0xa0 /* Escape Timer Register */
#define UBIR  0xa4 /* BRM Incremental Register */
#define UBMR  0xa8 /* BRM Modulator Register */
#define UBRC  0xac /* Baud Rate Count Register */
#define UTS   0xb4 /* UART Test Register (mx31) */


#define  UCR2_ESCI     (1<<15) /* Escape seq interrupt enable */
#define  UCR2_IRTS     (1<<14) /* Ignore RTS pin */
#define  UCR2_CTSC     (1<<13) /* CTS pin control */
#define  UCR2_CTS        (1<<12) /* Clear to send */
#define  UCR2_ESCEN      (1<<11) /* Escape enable */
#define  UCR2_PREN       (1<<8)  /* Parity enable */
#define  UCR2_PROE       (1<<7)  /* Parity odd/even */
#define  UCR2_STPB       (1<<6)     /* Stop */
#define  UCR2_WS         (1<<5)     /* Word size */
#define  UCR2_RTSEN      (1<<4)     /* Request to send interrupt enable */
#define  UCR2_TXEN       (1<<2)     /* Transmitter enabled */
#define  UCR2_RXEN       (1<<1)     /* Receiver enabled */
#define  UCR2_SRST     (1<<0)     /* SW reset */

#define  UCR1_UARTEN     (1<<0)     /* UART enabled */
#define  USR1_TRDY       (1<<13) /* TX Buffer ready */
#define  USR2_TXFE (1<<14)

int imx_uart_init(__iomem base_addr, unsigned int baudrate);
void imx_uart_putc(char c);

#endif  // PLAT_IMX_IMX_UART_H_
