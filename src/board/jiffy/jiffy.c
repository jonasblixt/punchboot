/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */


#include <board.h>
#include <plat.h>
#include <tinyprintf.h>
#include <io.h>
#include <gpt.h>
#include <image.h>
#include <boot.h>
#include <fuse.h>

#include <plat/imx6ul/plat.h>
#include <plat/imx6ul/imx_regs.h>
#include <plat/imx6ul/imx_uart.h>
#include <plat/imx6ul/ehci.h>
#include <plat/imx6ul/gpt.h>
#include <plat/imx6ul/ocotp.h>

#include "board_config.h"

const uint8_t part_type_config[] = 
{
    0xF7, 0xDD, 0x45, 0x34, 0xCC, 0xA5, 0xC6, 0x45, 
    0xAA, 0x17, 0xE4, 0x10, 0xA5, 0x42, 0xBD, 0xB8
};

const uint8_t part_type_system_a[] = 
{
    0x59, 0x04, 0x49, 0x1E, 0x6D, 0xE8, 0x4B, 0x44, 
    0x82, 0x93, 0xD8, 0xAF, 0x0B, 0xB4, 0x38, 0xD1
};

const uint8_t part_type_system_b[] = 
{ 
    0x3C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};

const uint8_t part_type_root_a[] = 
{ 
    0x1C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};

const uint8_t part_type_root_b[] = 
{ 
    0x2C, 0x29, 0x85, 0x3F, 0xFB, 0xC6, 0xD0, 0x42, 
    0x9E, 0x1A, 0xAC, 0x6B, 0x35, 0x60, 0xC3, 0x04
};


const struct fuse uuid_fuses[] =
{
    IMX6UL_FUSE_BANK_WORD(15, 4, "UUID0"),
    IMX6UL_FUSE_BANK_WORD(15, 5, "UUID1"),
    IMX6UL_FUSE_BANK_WORD(15, 6, "UUID2"),
    IMX6UL_FUSE_BANK_WORD(15, 7, "UUID3"),
    IMX6UL_FUSE_END,
};

const struct fuse device_info_fuses[] =
{
    IMX6UL_FUSE_BANK_WORD_VAL(15, 3, "Device Info", JIFFY_DEVICE_ID),
    IMX6UL_FUSE_END,
};

const struct fuse root_hash_fuses[] =
{
    IMX6UL_FUSE_BANK_WORD(3, 0, "SRK0"),
    IMX6UL_FUSE_BANK_WORD(3, 1, "SRK1"),
    IMX6UL_FUSE_BANK_WORD(3, 2, "SRK2"),
    IMX6UL_FUSE_BANK_WORD(3, 3, "SRK3"),
    IMX6UL_FUSE_BANK_WORD(3, 4, "SRK4"),
    IMX6UL_FUSE_BANK_WORD(3, 5, "SRK5"),
    IMX6UL_FUSE_BANK_WORD(3, 6, "SRK6"),
    IMX6UL_FUSE_BANK_WORD(3, 7, "SRK7"),
    IMX6UL_FUSE_END,
};


const struct fuse board_fuses[] =
{
    IMX6UL_FUSE_BANK_WORD_VAL(0, 5, "BOOT Config",        0x0000c060),
    IMX6UL_FUSE_BANK_WORD_VAL(0, 6, "BOOT from fuse bit", 0x00000010),
    IMX6UL_FUSE_END,
};


static struct ehci_device ehcidev = 
{
    .base = EHCI_PHY_BASE,
};

static struct usb_device usbdev =
{
    .platform_data = &ehcidev,
};

uint32_t board_usb_init(struct usb_device **dev)
{
    *dev = &usbdev;
    return PB_OK;
}

uint32_t board_get_debug_uart(void)
{
    return UART2_BASE;
}

uint32_t board_init(void)
{
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

uint8_t board_force_recovery(void) 
{
    uint8_t force_recovery = false;
    uint32_t err;
    struct fuse * boot_fuse = (struct fuse *) &board_fuses[0];

    /* Check force recovery input switch */
    if ( (pb_read32(0x020A8008) & (1 << 4)) == 0)
        force_recovery = true;
 
    err = plat_fuse_read(boot_fuse);

    if (err != PB_OK)
    {
        force_recovery = true;


        if (boot_fuse->value != boot_fuse->default_value)
            force_recovery = true;
    }

    if (force_recovery) 
        LOG_ERR ("OTP not set, forcing recovery mode");

    return force_recovery;
}

uint32_t board_configure_gpt_tbl(void) 
{
    gpt_add_part(1, 32768,  part_type_system_a, "System A");
    gpt_add_part(2, 32768,  part_type_system_b, "System B");
    gpt_add_part(3, 512000, part_type_root_a,   "Root A");
    gpt_add_part(4, 512000, part_type_root_b,   "Root B");

    return PB_OK;
}



