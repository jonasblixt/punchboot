/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb/board.h>
#include <pb/plat.h>
#include <pb/io.h>
#include <pb/gpt.h>
#include <pb/image.h>
#include <pb/boot.h>
#include <pb/fuse.h>
#include <plat/imx6ul/plat.h>
#include <plat/regs.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx/ehci.h>

const struct fuse fuses[] =
{
    IMX6UL_FUSE_BANK_WORD_VAL(3, 0, "SRK0", 0x5020C7D7),
    IMX6UL_FUSE_BANK_WORD_VAL(3, 1, "SRK1", 0xBB62B945),
    IMX6UL_FUSE_BANK_WORD_VAL(3, 2, "SRK2", 0xDD97C8BE),
    IMX6UL_FUSE_BANK_WORD_VAL(3, 3, "SRK3", 0xDC6710DD),
    IMX6UL_FUSE_BANK_WORD_VAL(3, 4, "SRK4", 0x2756B777),
    IMX6UL_FUSE_BANK_WORD_VAL(3, 5, "SRK5", 0xEF43BC0A),
    IMX6UL_FUSE_BANK_WORD_VAL(3, 6, "SRK6", 0x7185604B),
    IMX6UL_FUSE_BANK_WORD_VAL(3, 7, "SRK7", 0x3F335991),
    IMX6UL_FUSE_BANK_WORD_VAL(0, 5, "BOOT Config", 0x0000c060),
    IMX6UL_FUSE_BANK_WORD_VAL(0, 6, "BOOT from fuse bit", 0x00000010),
    IMX6UL_FUSE_END,
};


const struct partition_table pb_partition_table[] =
{
    PB_GPT_ENTRY(62768, PB_PARTUUID_SYSTEM_A, "System A"),
    PB_GPT_ENTRY(62768, PB_PARTUUID_SYSTEM_B, "System B"),
    PB_GPT_ENTRY(0x40000, PB_PARTUUID_ROOT_A, "Root A"),
    PB_GPT_ENTRY(0x40000, PB_PARTUUID_ROOT_B, "Root B"),
    PB_GPT_ENTRY(1, PB_PARTUUID_CONFIG_PRIMARY, "Config Primary"),
    PB_GPT_ENTRY(1, PB_PARTUUID_CONFIG_BACKUP, "Config Backup"),
    PB_GPT_END,
};

int board_early_init(void *plat)
{
    plat->wdog.base = 0x020BC000;
    plat->usb0.base = EHCI_PHY_BASE,
    plat->tmr0.base = 0x02098000;
    plat->tmr0.pr = 24;

    plat->uart0.base = UART2_BASE;
    plat->uart0.baudrate = 80000000L / (2 * 115200);

    plat->usdhc0.base = 0x02190000;
    plat->usdhc0.clk_ident = 0x10E1;
    plat->usdhc0.clk = 0x0101;
    plat->usdhc0.bus_mode = USDHC_BUS_DDR52;
    plat->usdhc0.bus_width = USDHC_BUS_8BIT;
    plat->usdhc0.boot_bus_cond = 0;

    /* Configure UART */
    pb_write32(0, 0x020E0094);
    pb_write32(0, 0x020E0098);
    pb_write32(UART_PAD_CTRL, 0x020E0320);
    pb_write32(UART_PAD_CTRL, 0x020E0324);

   /* Configure NAND_DATA2 as GPIO4 4 Input with PU,
    *
    * This is used to force recovery mode
    *
    **/

    pb_write32(5, 0x020E0188);
    pb_write32(0x2000 | (1 << 14) | (1 << 12), 0x020E0414);

    /* Configure pinmux for usdhc1 */
    pb_write32(0, 0x020E0000+0x1C0); /* CLK MUX */
    pb_write32(0, 0x020E0000+0x1BC); /* CMD MUX */
    pb_write32(0, 0x020E0000+0x1C4); /* DATA0 MUX */
    pb_write32(0, 0x020E0000+0x1C8); /* DATA1 MUX */
    pb_write32(0, 0x020E0000+0x1CC); /* DATA2 MUX */
    pb_write32(0, 0x020E0000+0x1D0); /* DATA3 MUX */
    pb_write32(1, 0x020E0000+0x1A8); /* DATA4 MUX */
    pb_write32(1, 0x020E0000+0x1AC); /* DATA5 MUX */
    pb_write32(1, 0x020E0000+0x1B0); /* DATA6 MUX */
    pb_write32(1, 0x020E0000+0x1B4); /* DATA7 MUX */
    pb_write32(1, 0x020E0000+0x1A4); /* RESET MUX */

    return PB_OK;
}

bool board_force_recovery(void *plat)
{
    uint8_t force_recovery = false;
    UNUSED(plat);

    /* Check force recovery input switch */
    if ( (pb_read32(0x020A8008) & (1 << 4)) == 0)
        force_recovery = true;

    return force_recovery;
}

int board_late_init(void *plat)
{
    UNUSED(plat);
    return PB_OK;
}

