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
#include <pb/uuid.h>
#include <pb/fuse.h>
#include <pb/gpt.h>
#include <pb/params.h>
#include <pb/board.h>
#include <pb/storage.h>
#include <pb/transport.h>
#include <plat/imx/ehci.h>
#include <plat/imx/usdhc.h>
#include <plat/defs.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/imx8x/plat.h>
#include <plat/imx8x/usdhc.h>
#include <plat/imx8x/ehci.h>
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
    PB_STORAGE_MAP2("9eef7544-bf68-4bf7-8678-da117cbccba8",
        "\x9e\xef\x75\x44\xbf\x68\x4b\xf7\x86\x78\xda\x11\x7c\xbc\xcb\xa8",
        "eMMC boot", 2048, DEF_FLAGS | PB_STORAGE_MAP_FLAG_EMMC_BOOT | \
                        PB_STORAGE_MAP_FLAG_STATIC_MAP),

    PB_STORAGE_MAP("2af755d8-8de5-45d5-a862-014cfa735ce0", "System A", 0xf000,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("c046ccd8-0f2e-4036-984d-76c14dc73992", "System B", 0xf000,
            DEF_FLAGS | PB_STORAGE_MAP_FLAG_BOOTABLE),

    PB_STORAGE_MAP("c284387a-3377-4c0f-b5db-1bcbcff1ba1a", "Root A", 0x40000,
            DEF_FLAGS),

    PB_STORAGE_MAP("ac6a1b62-7bd0-460b-9e6a-9a7831ccbfbb", "Root B", 0x40000,
            DEF_FLAGS),

    PB_STORAGE_MAP("f5f8c9ae-efb5-4071-9ba9-d313b082281e", "Config Primary",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP("656ab3fc-5856-4a5e-a2ae-5a018313b3ee", "Config Backup",
            1, PB_STORAGE_MAP_FLAG_VISIBLE),

    PB_STORAGE_MAP_END
};

/* USDHC0 driver configuration */

static uint8_t gpt_private_data[4096*9] __no_bss __a4k;
static uint8_t usdhc0_plat_private_data[4096] __no_bss __a4k;
static uint8_t usdhc0_dev_private_data[4096*4] __no_bss __a4k;
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

static struct pb_storage_platform_driver usdhc0_plat =
{
    .private = usdhc0_plat_private_data,
    .size = sizeof(usdhc0_plat_private_data),
};

static struct pb_storage_map_driver usdhc0_map_driver =
{
    .map_data = usdhc0_map_data,
    .map_size = sizeof(usdhc0_map_data),
    .private = gpt_private_data,
    .size = sizeof(gpt_private_data),
};

static struct pb_storage_driver usdhc0_driver =
{
    .name = "eMMC0",
    .block_size = 512,
    .default_map = map,
    .map = &usdhc0_map_driver,
    .platform = &usdhc0_plat,
    .private = &usdhc0,
};

/* END of USDHC0 */

/* USB0 transport driver */

static uint8_t usb0_dev_private_data[4096*65] __no_bss __a4k;
static uint8_t usb0_plat_private_data[4096] __no_bss __a4k;

static struct imx_ehci_device usb0_imx_ehci =
{
    .base = 0x5b0d0000,
    .private = &usb0_dev_private_data,
    .size = sizeof(usb0_dev_private_data),
};

static struct pb_transport_plat_driver usb0_plat_driver =
{
    .private = &usb0_plat_private_data,
    .size = sizeof(usb0_plat_private_data),
};

static struct pb_transport_driver usb0_driver =
{
    .name = "usb0",
    .platform = &usb0_plat_driver,
    .private = &usb0_imx_ehci,
    .size = sizeof(usb0_imx_ehci),
};

/* END of USB0*/

int board_early_init(struct pb_platform_setup *plat,
                     struct pb_storage *storage,
                     struct pb_transport *transport)
{
    int rc = PB_OK;
    sc_pm_clock_rate_t rate;

    plat->uart0.base = 0x5A060000;
    plat->uart0.baudrate = 0x402008b;

    /* Power up UART0 */
    sc_pm_set_resource_power_mode(plat->ipc_handle, SC_R_UART_0,
                                                    SC_PM_PW_MODE_ON);

    /* Set UART0 clock root to 80 MHz */
    rate = 80000000;
    sc_pm_set_clock_rate(plat->ipc_handle, SC_R_UART_0, SC_PM_CLK_PER, &rate);

    /* Enable UART0 clock root */
    sc_pm_clock_enable(plat->ipc_handle, SC_R_UART_0, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    sc_pad_set(plat->ipc_handle, SC_P_UART0_RX, UART_PAD_CTRL);
    sc_pad_set(plat->ipc_handle, SC_P_UART0_TX, UART_PAD_CTRL);

    /* Setup GPT0 */
    sc_pm_set_resource_power_mode(plat->ipc_handle, SC_R_GPT_0,
                                                    SC_PM_PW_MODE_ON);
    rate = 24000000;
    sc_pm_set_clock_rate(plat->ipc_handle, SC_R_GPT_0, 2, &rate);

    rc = sc_pm_clock_enable(plat->ipc_handle, SC_R_GPT_0, 2, true, false);

    if (rc != SC_ERR_NONE)
        return -PB_ERR;

    plat->tmr0.base = 0x5D140000;
    plat->tmr0.pr = 24;

    /* Configure storage */

    rc = imx8x_usdhc_setup(&usdhc0_driver, plat->ipc_handle);

    if (rc != PB_OK)
        return rc;

    rc = pb_gpt_map_init(&usdhc0_driver);

    if (rc != PB_OK)
        return rc;

    rc = pb_storage_add(storage, &usdhc0_driver);

    if (rc != PB_OK)
        return rc;

    /* Configure command transport */

    rc = imx8x_ehci_setup(&usb0_driver, plat->ipc_handle);

    if (rc != PB_OK)
        return rc;

    rc = pb_transport_add(transport, &usb0_driver);

    return PB_OK;
}

uint32_t board_late_init(struct pb_platform_setup *plat)
{
    UNUSED(plat);
    return PB_OK;
}

uint32_t board_get_params(struct param **pp)
{
    param_add_str((*pp)++, "Board", "IMX8QXPMEK");
    return PB_OK;
}

uint32_t board_setup_device(struct param *params)
{
    UNUSED(params);
    return PB_OK;
}

bool board_force_recovery(struct pb_platform_setup *plat)
{
    sc_bool_t btn_status;
    sc_misc_bt_t boot_type;
    bool usb_charger_detected = false;

    sc_misc_get_button_status(plat->ipc_handle, &btn_status);
    sc_misc_get_boot_type(plat->ipc_handle, &boot_type);

    LOG_INFO("Boot type: %u", boot_type);

    /* Pull up DP for usb charger detection */
    pb_setbit32(1 << 2, 0x5b100000+0xe0);
    LOG_DBG ("USB CHRG detect: 0x%08x",pb_read32(0x5B100000+0xf0));
    plat_delay_ms(1);
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



uint32_t board_linux_patch_dt(void *fdt, int offset)
{
    UNUSED(fdt);
    UNUSED(offset);

    return PB_OK;
}

uint32_t board_recovery_command(uint32_t arg0, uint32_t arg1, uint32_t arg2,
                                uint32_t arg3)
{
    UNUSED(arg0);
    UNUSED(arg1);
    UNUSED(arg2);
    UNUSED(arg3);

    return PB_ERR;
}

