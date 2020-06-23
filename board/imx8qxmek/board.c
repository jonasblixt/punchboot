/**
 * Punch BOOT
 *
 * Copyright (C) 2018 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <stdio.h>
#include <stdbool.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <pb/plat.h>
#include <pb/usb.h>
#include <pb/fuse.h>
#include <pb/gpt.h>
#include <pb/board.h>
#include <pb/storage.h>
#include <pb/boot.h>
#include <plat/imx/ehci.h>
#include <plat/imx/usdhc.h>
#include <plat/defs.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/imx8x/plat.h>
#include <uuid/uuid.h>
#include <libfdt.h>

const struct fuse fuses[] =
{
    IMX8X_FUSE_ROW_VAL(730, "SRK0", 0x6147e2e6),
    IMX8X_FUSE_ROW_VAL(731, "SRK1", 0xfc4dc849),
    IMX8X_FUSE_ROW_VAL(732, "SRK2", 0xb410b214),
    IMX8X_FUSE_ROW_VAL(733, "SRK3", 0x0f8d6212),
    IMX8X_FUSE_ROW_VAL(734, "SRK4", 0xad38b486),
    IMX8X_FUSE_ROW_VAL(735, "SRK5", 0x9b806149),
    IMX8X_FUSE_ROW_VAL(736, "SRK6", 0xdd6d397a),
    IMX8X_FUSE_ROW_VAL(737, "SRK7", 0x4c19d87b),
    IMX8X_FUSE_ROW_VAL(738, "SRK8", 0x24ac2acd),
    IMX8X_FUSE_ROW_VAL(739, "SRK9", 0xb6222a62),
    IMX8X_FUSE_ROW_VAL(740, "SRK10", 0xf36d6bd1),
    IMX8X_FUSE_ROW_VAL(741, "SRK11", 0x14cc8e16),
    IMX8X_FUSE_ROW_VAL(742, "SRK12", 0xd749170e),
    IMX8X_FUSE_ROW_VAL(743, "SRK13", 0x22fb187e),
    IMX8X_FUSE_ROW_VAL(744, "SRK14", 0x158f740c),
    IMX8X_FUSE_ROW_VAL(745, "SRK15", 0x8966b0f6),
    IMX8X_FUSE_ROW_VAL(18, "BOOT Config",  0x00000002),
    IMX8X_FUSE_ROW_VAL(19, "Bootconfig2" , 0x00000025),
    IMX8X_FUSE_END,
};

#define DEF_FLAGS (PB_STORAGE_MAP_FLAG_WRITABLE | \
                   PB_STORAGE_MAP_FLAG_VISIBLE)

const struct pb_storage_map map[] =
{
    PB_STORAGE_MAP("9eef7544-bf68-4bf7-8678-da117cbccba8",
        "eMMC boot0", 2048, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT0 | \
                        PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP("4ee31690-0c9b-4d56-a6a6-e6d6ecfd4d54",
        "eMMC boot1", 2048, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT1 | \
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

const uint32_t rom_key_map[] =
{
    0xa90f9680,
    0x25c6dd36,
    0x52c1eda0,
    0xcca57803,
    0x00000000,
};

/* USDHC0 driver configuration */

static uint8_t usdhc0_dev_private_data[4096*4] __no_bss __a4k;
static uint8_t usdhc0_gpt_map_data[4096*10] __no_bss __a4k;
static uint8_t usdhc0_map_data[4096*4] __no_bss __a4k;

static const struct usdhc_device usdhc0 =
{
    .base = 0x5B010000,
    .clk_ident = 0x08EF,
    .clk = 0x000F,
    .bus_mode = USDHC_BUS_HS200,
    .bus_width = USDHC_BUS_8BIT,
    .boot_bus_cond = 0x12, /* Enable fastboot 8-bit DDR */
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
        bootargs = "console=ttyLP0,115200  " \
                   "earlycon=adma_lpuart32,0x5a060000,115200 earlyprintk " \
                   "fec.macaddr=0xe2,0xf4,0x91,0x3a,0x82,0x93 ";
    }
    else
    {
        bootargs = "console=ttyLP0,115200  " \
                   "quiet " \
                   "fec.macaddr=0xe2,0xf4,0x91,0x3a,0x82,0x93 ";
    }
    return fdt_setprop_string(fdt, offset, "bootargs", bootargs);

}

int board_early_init(void *plat)
{
    int rc = PB_OK;

    rc = pb_storage_add(&usdhc0_driver);

    if (rc != PB_OK)
        return rc;

    return PB_OK;
}

int board_setup_device(void)
{
    return PB_OK;
}

bool board_force_command_mode(void *plat)
{
    struct imx8x_private *priv = IMX8X_PRIV(plat);

    sc_bool_t btn_status;
    sc_misc_bt_t boot_type;
    bool usb_charger_detected = false;

    sc_misc_get_button_status(priv->ipc, &btn_status);
    sc_misc_get_boot_type(priv->ipc, &boot_type);

    LOG_INFO("Boot type: %u %i %p", boot_type, btn_status, priv);

    /* Pull up DP for usb charger detection */

    pb_setbit32(1 << 2, 0x5b100000+0xe0);
    LOG_DBG ("USB CHRG detect: 0x%08x",pb_read32(0x5B100000+0xf0));
    pb_delay_ms(1);
    if ((pb_read32(0x5b100000+0xf0) & 0x0C) == 0x0C)
        usb_charger_detected = true;
    pb_clrbit32(1 << 2, 0x5b100000+0xe0);

    if (usb_charger_detected)
    {
        LOG_INFO("USB Charger condition, entering bootloader");
    }

    return (btn_status == 1) || (boot_type == SC_MISC_BT_SERIAL) ||
            (usb_charger_detected);
}

const char * board_name(void)
{
    return "imx8qxmek";
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
    struct imx8x_private *priv = IMX8X_PRIV(plat);
    uint32_t scu_version;
    uint32_t scu_commit;
    uint32_t seco_version;
    uint32_t seco_commit;
    int16_t celsius;
    int8_t tenths;

    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    sc_misc_build_info(priv->ipc, &scu_version, &scu_commit);
    sc_misc_seco_build_info(priv->ipc, &seco_version, &seco_commit);
    sc_misc_get_temp(priv->ipc, SC_R_SYSTEM, SC_MISC_TEMP, &celsius,
                        &tenths);

    (*response_size) = snprintf(response, resp_buf_size,
                            "SCFW: %u, %x\n" \
                            "SECO: %u, %x\n" \
                            "CPU Temperature: %i.%i deg C\n",
                            scu_version, scu_commit,
                            seco_version, seco_commit,
                            celsius, tenths);

    response[(*response_size)++] = 0;

    return PB_OK;
}

