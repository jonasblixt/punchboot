
/**
 * Punch BOOT
 *
 * Copyright (C) 2020 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <pb/plat.h>
#include <pb/usb.h>
#include <pb/fuse.h>
#include <pb/gpt.h>
#include <plat/imx/dwc3.h>
#include <plat/imx/usdhc.h>
#include <plat/imx8m/plat.h>
#include <libfdt.h>

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
        "eMMC boot0", 66, 4162, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT0 | \
                        PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP3("4ee31690-0c9b-4d56-a6a6-e6d6ecfd4d54",
        "eMMC boot1", 66, 4162, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT1 | \
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
    .base = 0x30B40000,
    .clk_ident = 0x20EF,
    .clk = 0x000F,
    .bus_mode = USDHC_BUS_HS200,
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

int board_early_init(void *plat)
{
    int rc;

    LOG_DBG("Hello %p %zu", &usdhc0_driver, usdhc0_driver.block_size);
    rc = pb_storage_add(&usdhc0_driver);
    LOG_DBG("rc = %i", rc);

    if (rc != PB_OK)
        return rc;

    LOG_DBG("Board init done");
    return PB_OK;
}

const char *board_name(void)
{
    return "pico8ml";
}

int board_command(void *plat,
                     uint32_t command,
                     void *bfr,
                     size_t size,
                     void *response_bfr,
                     size_t *response_size)
{
    LOG_DBG("%x, %p, %zu", command, bfr, size);

    if (command == 0xf93ba110)
    {
        LOG_DBG("Got test command");
        char *response = (char *) response_bfr;
        size_t resp_buf_size = *response_size;

        (*response_size) = snprintf(response, resp_buf_size,
                                    "Test command hello 0x%x\n", command);

        response[(*response_size)++] = 0;
    }
    else
    {
        *response_size = 0;
    }

    return PB_OK;
}

int board_status(void *plat,
                    void *response_bfr,
                    size_t *response_size)
{
    struct imx8m_private *priv = IMX8M_PRIV(plat);

    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;
    const char *soc_major_var = "?";
    const char *soc_minor_var = "?";
    const char *soc_no_of_cores = "?";
    unsigned int base_ver = 0;
    unsigned int metal_ver = 0;

    switch ((priv->soc_ver_var >> 16) & 0x0f)
    {
        case 0x02:
            soc_major_var = "M";
        break;
        default:
            soc_major_var = "?";
        break;
    }

    switch((priv->soc_ver_var >> 12) & 0x0f)
    {
        case 0x04:
            soc_no_of_cores = "quad";
        break;
        case 0x02:
            soc_no_of_cores = "dual";
        break;
        default:
            soc_no_of_cores = "?";
        break;
    }

    switch((priv->soc_ver_var >> 8) & 0x0f)
    {
        case 0x00:
            soc_minor_var = "lite";
        break;
        default:
            soc_minor_var = "?";
        break;
    }

    base_ver = (priv->soc_ver_var >> 4) & 0x0f;
    metal_ver = (priv->soc_ver_var) & 0x0f;


    (*response_size) = snprintf(response, resp_buf_size,
                            "SOC: IMX8%s %s-%s, %i.%i\n",
                            soc_major_var,
                            soc_no_of_cores,
                            soc_minor_var,
                            base_ver,
                            metal_ver);

    response[(*response_size)++] = 0;

    return PB_OK;
}

bool board_force_command_mode(void *plat)
{
    return true;
}

int board_patch_bootargs(void *plat, void *fdt, int offset, bool verbose_boot)
{
    const char *bootargs = NULL;

    if (verbose_boot)
    {
        bootargs = "console=ttymxc0,115200 " \
                         "earlyprintk ";
    }
    else
    {
        bootargs = "console=ttymxc0,115200 " \
                         "quiet ";
    }

    return fdt_setprop_string(fdt, offset, "bootargs", bootargs);
}
