/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Most of this is copied from ATF
 *
 */

#include <drivers/uart/imx_lpuart.h>
#include <pb/mmio.h>

#define VERID                 0x0
#define LPPARAM               0x4
#define GLOBAL                0x8
#define PINCFG                0xC
#define BAUD                  0x10
#define STAT                  0x14
#define CTRL                  0x18
#define DATA                  0x1C
#define MATCH                 0x20
#define MODIR                 0x24
#define FIFO                  0x28
#define WATER                 0x2c

#define US1_TDRE              (1 << 23)
#define US1_RDRF              (1 << 21)

#define CTRL_TE               (1 << 19)
#define CTRL_RE               (1 << 18)

#define FIFO_TXFE             0x80
#define FIFO_RXFE             0x40

#define WATER_TXWATER_OFF     1
#define WATER_RXWATER_OFF     16

#define LPUART_CTRL_PT_MASK   0x1
#define LPUART_CTRL_PE_MASK   0x2
#define LPUART_CTRL_M_MASK    0x10

#define LPUART_BAUD_OSR_MASK  (0x1F000000U)
#define LPUART_BAUD_OSR_SHIFT (24U)
#define LPUART_BAUD_OSR(x) \
    (((uint32_t)(((uint32_t)(x)) << LPUART_BAUD_OSR_SHIFT)) & LPUART_BAUD_OSR_MASK)

#define LPUART_BAUD_SBR_MASK  (0x1FFFU)
#define LPUART_BAUD_SBR_SHIFT (0U)
#define LPUART_BAUD_SBR(x) \
    (((uint32_t)(((uint32_t)(x)) << LPUART_BAUD_SBR_SHIFT)) & LPUART_BAUD_SBR_MASK)

#define LPUART_BAUD_SBNS_MASK     (0x2000U)
#define LPUART_BAUD_BOTHEDGE_MASK (0x20000U)
#define LPUART_BAUD_M10_MASK      (0x20000000U)

static void lpuart_setbrg(uintptr_t base, unsigned int rate, int baudrate)
{
    unsigned int sbr, osr, baud_diff, tmp_osr, tmp_sbr;
    unsigned int diff1, diff2, tmp;

    baud_diff = baudrate;
    osr = 0;
    sbr = 0;
    for (tmp_osr = 4; tmp_osr <= 32; tmp_osr++) {
        tmp_sbr = (rate / (baudrate * tmp_osr));
        if (tmp_sbr == 0)
            tmp_sbr = 1;

        /* calculate difference in actual baud w/ current values */
        diff1 = rate / (tmp_osr * tmp_sbr) - baudrate;
        diff2 = rate / (tmp_osr * (tmp_sbr + 1));

        /* select best values between sbr and sbr+1 */
        if (diff1 > (baudrate - diff2)) {
            diff1 = baudrate - diff2;
            tmp_sbr++;
        }

        if (diff1 <= baud_diff) {
            baud_diff = diff1;
            osr = tmp_osr;
            sbr = tmp_sbr;
        }
    }

    tmp = mmio_read_32(base + BAUD);

    if ((osr > 3) && (osr < 8))
        tmp |= LPUART_BAUD_BOTHEDGE_MASK;

    tmp &= ~LPUART_BAUD_OSR_MASK;
    tmp |= LPUART_BAUD_OSR(osr - 1);
    tmp &= ~LPUART_BAUD_SBR_MASK;
    tmp |= LPUART_BAUD_SBR(sbr);

    /* explicitly disable 10 bit mode & set 1 stop bit */
    tmp &= ~(LPUART_BAUD_M10_MASK | LPUART_BAUD_SBNS_MASK);

    mmio_write_32(base + BAUD, tmp);
}

void imx_lpuart_putc(uintptr_t base, char c)
{
    while (!(mmio_read_32(base + STAT) & (1 << 22)))
        ;

    mmio_write_32(base + DATA, c);
}

int imx_lpuart_init(uintptr_t base, unsigned int input_clock_Hz, unsigned int baudrate)
{
    mmio_clrsetbits_32(base + CTRL, CTRL_TE | CTRL_RE, 0);
    mmio_write_32(base + MODIR, 0);
    mmio_clrsetbits_32(base + FIFO, FIFO_TXFE | FIFO_RXFE, 0);
    mmio_write_32(base + MATCH, 0);
    lpuart_setbrg(base, input_clock_Hz, baudrate);
    mmio_clrsetbits_32(base + CTRL,
                       LPUART_CTRL_PE_MASK | LPUART_CTRL_PT_MASK | LPUART_CTRL_M_MASK,
                       CTRL_RE | CTRL_TE);
    return 0;
}
