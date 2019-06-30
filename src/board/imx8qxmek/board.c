#include <stdio.h>
#include <pb.h>
#include <io.h>
#include <plat.h>
#include <stdbool.h>
#include <usb.h>
#include <uuid.h>
#include <fuse.h>
#include <gpt.h>
#include <params.h>
#include <plat/imx/ehci.h>
#include <plat/imx/usdhc.h>
#include <plat/defs.h>
#include <plat/sci/ipc.h>
#include <plat/sci/sci.h>
#include <plat/imx8qxp_pads.h>
#include <plat/imx8x/plat.h>
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
    IMX8X_FUSE_ROW_VAL(740, "SRK10",0xf36d6bd1),
    IMX8X_FUSE_ROW_VAL(741, "SRK11",0x14cc8e16),
    IMX8X_FUSE_ROW_VAL(742, "SRK12",0xd749170e),
    IMX8X_FUSE_ROW_VAL(743, "SRK13",0x22fb187e),
    IMX8X_FUSE_ROW_VAL(744, "SRK14",0x158f740c),
    IMX8X_FUSE_ROW_VAL(745, "SRK15",0x8966b0f6),
    IMX8X_FUSE_ROW_VAL(18, "BOOT Config",  0x00000002),
    IMX8X_FUSE_ROW_VAL(19, "Bootconfig2" , 0x00000025),
    IMX8X_FUSE_END,
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
    uint32_t err = PB_OK;
    sc_pm_clock_rate_t rate;

    plat->usb.base = 0x5b0d0000;

    plat->uart0.base = 0x5A060000;
    plat->uart0.baudrate = 0x402008b;

	/* Power up UART0 */
	sc_pm_set_resource_power_mode(plat->ipc_handle, SC_R_UART_0, SC_PM_PW_MODE_ON);

	/* Set UART0 clock root to 80 MHz */
	rate = 80000000;
	sc_pm_set_clock_rate(plat->ipc_handle, SC_R_UART_0, 2, &rate);

	/* Enable UART0 clock root */
	sc_pm_clock_enable(plat->ipc_handle, SC_R_UART_0, 2, true, false);

	/* Configure UART pads */
	sc_pad_set(plat->ipc_handle, SC_P_UART0_RX, UART_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_UART0_TX, UART_PAD_CTRL);

    /* Setup GPT0 */
	sc_pm_set_resource_power_mode(plat->ipc_handle, SC_R_GPT_0, SC_PM_PW_MODE_ON);
	rate = 24000000;
	sc_pm_set_clock_rate(plat->ipc_handle, SC_R_GPT_0, 2, &rate);

	err = sc_pm_clock_enable(plat->ipc_handle, SC_R_GPT_0, 2, true, false);

    if (err != SC_ERR_NONE)
    {
        LOG_ERR("Could not enable GPT0 clock");
        return PB_ERR;
    }

    plat->tmr0.base = 0x5D140000;
    plat->tmr0.pr = 24;

    /* Setup USDHC0 */
	sc_pm_set_resource_power_mode(plat->ipc_handle, SC_R_SDHC_0, SC_PM_PW_MODE_ON);


	sc_pm_clock_enable(plat->ipc_handle, SC_R_SDHC_0, SC_PM_CLK_PER, 
                                false, false);

    err = sc_pm_set_clock_parent(plat->ipc_handle, SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (err != SC_ERR_NONE)
    {
        LOG_ERR("usdhc set clock parent failed");
        return PB_ERR;
    }

	rate = 200000000;
	sc_pm_set_clock_rate(plat->ipc_handle, SC_R_SDHC_0, 2, &rate);

    if (rate != 200000000)
    {
        LOG_INFO("USDHC rate %u Hz", rate);
    }

	err = sc_pm_clock_enable(plat->ipc_handle, SC_R_SDHC_0, SC_PM_CLK_PER, 
                                true, false);

	if (err != SC_ERR_NONE) 
    {
		LOG_ERR("SDHC_0 per clk enable failed!");
		return PB_ERR;
	}


	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_CLK, ESDHC_CLK_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_CMD, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA0, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA1, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA2, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA3, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA4, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA5, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA6, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_DATA7, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_STROBE, ESDHC_PAD_CTRL);
	sc_pad_set(plat->ipc_handle, SC_P_EMMC0_RESET_B, ESDHC_PAD_CTRL);

    plat->usdhc0.base = 0x5B010000;
    plat->usdhc0.clk_ident = 0x08EF;
    plat->usdhc0.clk = 0x000F;
    plat->usdhc0.bus_mode = USDHC_BUS_HS200;
    plat->usdhc0.bus_width = USDHC_BUS_8BIT;
    plat->usdhc0.boot_bus_cond = 0x12; /* Enable fastboot 8-bit DDR */

    return PB_OK;
}

uint32_t board_late_init(struct pb_platform_setup *plat)
{
    UNUSED(plat);   
    return PB_OK;
}

uint32_t board_prepare_recovery(struct pb_platform_setup *plat)
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
    //LOG_DBG ("USB CHRG detect: 0x%08x",pb_read32(0x5B100000+0xf0));
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



uint32_t board_linux_patch_dt (void *fdt, int offset)
{
    UNUSED(fdt);
    UNUSED(offset);

    return PB_OK;
}
