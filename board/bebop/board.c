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
#include <pb/arch.h>
#include <pb/io.h>
#include <pb/gpt.h>
#include <pb/image.h>
#include <pb/boot.h>
#include <pb/fuse.h>
#include <plat/imx6ul/plat.h>
#include <plat/defs.h>
#include <plat/regs.h>
#include <plat/imx/imx_uart.h>
#include <plat/imx/ehci.h>
#include <plat/imx/usdhc.h>
#include <arch/cp15.h>
#include <libfdt.h>

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

#define DEF_FLAGS (PB_STORAGE_MAP_FLAG_WRITABLE | \
                   PB_STORAGE_MAP_FLAG_VISIBLE)

const uint32_t rom_key_map[] =
{
    0xa90f9680,
    0x25c6dd36,
    0x52c1eda0,
    0xcca57803,
    0x00000000,
};

const struct pb_storage_map map[] =
{
    PB_STORAGE_MAP3("9eef7544-bf68-4bf7-8678-da117cbccba8",
        "eMMC boot0", 2, 2050, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT0 | \
                        PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP3("4ee31690-0c9b-4d56-a6a6-e6d6ecfd4d54",
        "eMMC boot1", 2, 2050, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT1 | \
                        PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP("2af755d8-8de5-45d5-a862-014cfa735ce0", "System A", 0xf000,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("c046ccd8-0f2e-4036-984d-76c14dc73992", "System B", 0xf000,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("c284387a-3377-4c0f-b5db-1bcbcff1ba1a", "Root A", 0x40000,
            DEF_FLAGS),

    PB_STORAGE_MAP("ac6a1b62-7bd0-460b-9e6a-9a7831ccbfbb", "Root B", 0x40000,
            DEF_FLAGS),

    PB_STORAGE_MAP("f5f8c9ae-efb5-4071-9ba9-d313b082281e", "PB State Primary",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP("656ab3fc-5856-4a5e-a2ae-5a018313b3ee", "PB State Backup",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP_END
};


/* USDHC0 driver configuration */

static uint8_t usdhc0_dev_private_data[4096*4] __no_bss __a4k;
static uint8_t usdhc0_gpt_map_data[4096*10] __no_bss __a4k;
static uint8_t usdhc0_map_data[4096*4] __no_bss __a4k;

static const struct usdhc_device usdhc0 =
{
    .base = 0x02190000,
    .clk_ident = 0x10E1,
    .clk = 0x0101,
    .bus_mode = USDHC_BUS_DDR52,
    .bus_width = USDHC_BUS_8BIT,
    .boot_bus_cond = 0x0, 
    .private = usdhc0_dev_private_data,
    .size = sizeof(usdhc0_dev_private_data),
};

static struct pb_storage_driver usdhc0_driver =
{
    .name = "eMMC0",
    .block_size = 512,
    .driver_private = &usdhc0,
    .init = imx_usdhc_init,
    .map_default = map,
    .map_init = gpt_init,
    .map_install = gpt_install_map,
    .map_private = usdhc0_gpt_map_data,
    .map_private_size = sizeof(usdhc0_map_data),
    .map_data = usdhc0_map_data,
    .map_data_size = sizeof(usdhc0_map_data),
};

/* END of USDHC0 */

int board_patch_bootargs(void *plat, void *fdt, int offset, bool verbose_boot)
{
    const char *bootargs = NULL;

    if (verbose_boot)
    {
        bootargs = "console=ttymxc1,115200 " \
                         "earlyprintk ";
    }
    else
    {
        bootargs = "console=ttymxc1,115200 " \
                         "quiet ";
    }
    return fdt_setprop_string(fdt, offset, "bootargs", bootargs);

}

int board_early_init(void *plat)
{
    int rc;
   /* Configure NAND_DATA2 as GPIO4 4 Input with PU,
    *
    * This is used to force recovery mode
    *
    **/

    pb_write32(5, 0x020E0188);
    pb_write32(0x2000 | (1 << 14) | (1 << 12), 0x020E0414);


    rc = pb_storage_add(&usdhc0_driver);

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

bool board_force_command_mode(void *plat)
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

const char *board_name(void)
{
    return "Bebop";
}

int board_command(void *plat,
                     uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);

    *response_size = 0;
    return PB_OK;
}

int board_status(void *plat,
                    void *response_bfr,
                    size_t *response_size)
{
    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    (*response_size) = snprintf(response, resp_buf_size,
                            "Board status OK!\n");
    response[(*response_size)++] = 0;

    return PB_OK;
}

int board_slc_set_configuration(void *plat)
{
    return PB_OK;
}

int board_slc_set_configuration_lock(void *plat)
{
    return PB_OK;
}
