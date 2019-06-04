
/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Persson <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <plat.h>
#include <stdbool.h>
#include <usb.h>
#include <fuse.h>
#include <gpt.h>
#include <plat/defs.h>
#include <plat/imx/dwc3.h>
#include <plat/imx/usdhc.h>
#include <plat/imx8m/plat.h>

const struct fuse fuses[] =
{
    IMX8M_FUSE_BANK_WORD_VAL(6, 0, "SRK0", 0x5020C7D7),
    IMX8M_FUSE_BANK_WORD_VAL(6, 1, "SRK1", 0xBB62B945),
    IMX8M_FUSE_BANK_WORD_VAL(6, 2, "SRK2", 0xDD97C8BE),
    IMX8M_FUSE_BANK_WORD_VAL(6, 3, "SRK3", 0xDC6710DD),
    IMX8M_FUSE_BANK_WORD_VAL(7, 0, "SRK4", 0x2756B777),
    IMX8M_FUSE_BANK_WORD_VAL(7, 1, "SRK5", 0xEF43BC0A),
    IMX8M_FUSE_BANK_WORD_VAL(7, 2, "SRK6", 0x7185604B),
    IMX8M_FUSE_BANK_WORD_VAL(7, 3, "SRK7", 0x3F335991),
    IMX8M_FUSE_BANK_WORD_VAL(1, 3, "BOOT Config",  0x00002060),
    IMX8M_FUSE_END,
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

uint32_t board_early_init(struct pb_platform_setup *plat)
{
    plat->uart0.base = 0x30860000;
    plat->usb0.base = 0x38100000;
    plat->tmr0.base = 0x302D0000;
    plat->tmr0.pr = 40;
    plat->uart0.baudrate = 0x6C;

    plat->usdhc0.base = 0x30B40000;
    plat->usdhc0.clk_ident = 0x20EF;
    plat->usdhc0.clk = 0x000F;
    plat->usdhc0.bus_mode = USDHC_BUS_HS200;
    plat->usdhc0.bus_width = USDHC_BUS_8BIT;
    plat->usdhc0.boot_bus_cond = 0;


    plat->ocotp.base = 0x30350000;
    plat->ocotp.words_per_bank = 4;

    /* Enable UART1 clock */
    pb_write32((1 << 28) ,0x30388004 + 94*0x80);
    /* Ungate UART1 clock */
    pb_write32(3, 0x30384004 + 0x10*73);

    /* Ungate GPIO blocks */

    pb_write32(3, 0x30384004 + 0x10*11);
    pb_write32(3, 0x30384004 + 0x10*12);
    pb_write32(3, 0x30384004 + 0x10*13);
    pb_write32(3, 0x30384004 + 0x10*14);
    pb_write32(3, 0x30384004 + 0x10*15);


    pb_write32(3, 0x30384004 + 0x10*27);
    pb_write32(3, 0x30384004 + 0x10*28);
    pb_write32(3, 0x30384004 + 0x10*29);
    pb_write32(3, 0x30384004 + 0x10*30);
    pb_write32(3, 0x30384004 + 0x10*31);


    /* UART1 pad mux */
    pb_write32(0, 0x30330234);
    pb_write32(0, 0x30330238);
    
    /* UART1 PAD settings */
    pb_write32(7, 0x3033049C);
    pb_write32(7, 0x303304A0);

    /* USDHC1 reset */
    /* Configure as GPIO 2 10*/
    pb_write32 (5, 0x303300C8);
    pb_write32((1 << 10), 0x30210004);

    pb_setbit32(1<<10, 0x30210000);

    /* USDHC1 mux */
    pb_write32 (0, 0x303300A0);
    pb_write32 (0, 0x303300A4);

    pb_write32 (0, 0x303300A8);
    pb_write32 (0, 0x303300AC);
    pb_write32 (0, 0x303300B0);
    pb_write32 (0, 0x303300B4);
    pb_write32 (0, 0x303300B8);
    pb_write32 (0, 0x303300BC);
    pb_write32 (0, 0x303300C0);
    pb_write32 (0, 0x303300C4);
    //pb_write32 (0, 0x303300C8);
    pb_write32 (0, 0x303300CC);

    /* Setup USDHC1 pins */
#define USDHC1_PAD_CONF ((1 << 7) | (1 << 6) | (2 << 3) | 6)
    pb_write32(USDHC1_PAD_CONF, 0x30330308);
    pb_write32(USDHC1_PAD_CONF, 0x3033030C);
    pb_write32(USDHC1_PAD_CONF, 0x30330310);
    pb_write32(USDHC1_PAD_CONF, 0x30330314);
    pb_write32(USDHC1_PAD_CONF, 0x30330318);
    pb_write32(USDHC1_PAD_CONF, 0x3033031C);
    pb_write32(USDHC1_PAD_CONF, 0x30330320);
    pb_write32(USDHC1_PAD_CONF, 0x30330324);
    pb_write32(USDHC1_PAD_CONF, 0x30330328);
    pb_write32(USDHC1_PAD_CONF, 0x3033032C);

    pb_write32(USDHC1_PAD_CONF, 0x30330334);
    pb_clrbit32(1<<10, 0x30210000);

    return PB_OK;
}


uint32_t board_get_params(struct param **pp)
{
    param_add_str((*pp)++, "Board", "Pico8ml");
    return PB_OK;
}

uint32_t board_setup_device(struct param *params)
{
    UNUSED(params);
    return PB_OK;
}

uint32_t board_prepare_recovery(struct pb_platform_setup *plat)
{
    UNUSED(plat);
    return PB_OK;
}

uint32_t board_late_init(struct pb_platform_setup *plat)
{
    UNUSED(plat);
    return PB_OK;
}

bool board_force_recovery(struct pb_platform_setup *plat)
{
    UNUSED(plat);
    return false;
}

uint32_t board_linux_patch_dt (void *fdt, int offset)
{
    UNUSED(fdt);
    UNUSED(offset);

    return PB_OK;
}
