/**
 * Punch BOOT
 *
 * Copyright (C) 2023 Jonas Blixt <jonpe960@gmail.com>
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */

#include <arch/arch_helpers.h>
#include <arch/armv8a/timer.h>
#include <boot/ab_state.h>
#include <boot/atf.h>
#include <boot/boot.h>
#include <drivers/crypto/imx_caam.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/uart/imx_lpuart.h>
#include <drivers/usb/imx_cdns3_udc.h>
#include <drivers/usb/imx_ci_udc.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/usb/pb_dev_cls.h>
#include <pb/board.h>
#include <pb/board_layout.h>
#include <pb/cm.h>
#include <pb/console.h>
#include <pb/crypto.h>
#include <pb/delay.h>
#include <pb/pb.h>
#include <pb/plat.h>
#include <pb/rot.h>
#include <pb/slc.h>
#include <plat/imx8x/fusebox.h>
#include <plat/imx8x/imx8x.h>
#include <sci/svc/misc/api.h>
#include <sci/svc/pad/api.h>
#include <sci/svc/pm/api.h>
#include <sci/svc/seco/api.h>
#include <stdbool.h>
#include <stdio.h>
#include <uuid.h>

#define USDHC_PAD_CTRL                                                                             \
    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
     (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) |                                                  \
     (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) |                                            \
     (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define USDHC_CLK_PAD_CTRL                                                                         \
    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
     (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) |                                                  \
     (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) |                                            \
     (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

static int usdhc_emmc_setup(void)
{
    int rc;
    unsigned int rate;

    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_SDHC_0, SC_PM_PW_MODE_ON);
    sc_pm_clock_enable(imx8x_ipc(), SC_R_SDHC_0, SC_PM_CLK_PER, false, false);

    rc = sc_pm_set_clock_parent(imx8x_ipc(), SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("usdhc set clock parent failed");
        return -PB_ERR;
    }

    rate = MHz(200);
    sc_pm_set_clock_rate(imx8x_ipc(), SC_R_SDHC_0, 2, &rate);

    rc = sc_pm_clock_enable(imx8x_ipc(), SC_R_SDHC_0, SC_PM_CLK_PER, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("SDHC_0 per clk enable failed!");
        return -PB_ERR;
    }

    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_CLK, USDHC_CLK_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_CMD, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA0, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA1, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA2, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA3, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA4, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA5, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA6, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_DATA7, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_STROBE, USDHC_PAD_CTRL);
    sc_pad_set(imx8x_ipc(), SC_P_EMMC0_RESET_B, USDHC_PAD_CTRL);

    static const struct imx_usdhc_config cfg = { .base = 0x5B010000,
                                                 .delay_tap = 18,
                                                 .mmc_config = {
                                                     .mode = MMC_BUS_MODE_HS200,
                                                     .width = MMC_BUS_WIDTH_8BIT,
                                                     .boot_mode = EXT_CSD_BOOT_DDR |
                                                                  EXT_CSD_BOOT_BUS_WIDTH_8,
                                                     .boot0_uu = PART_boot0,
                                                     .boot1_uu = PART_boot1,
                                                     .user_uu = PART_user,
                                                     .rpmb_uu = PART_rpmb,
                                                     .flags = 0,
                                                 } };

    return imx_usdhc_init(&cfg, rate);
}

static int early_boot(void)
{
    sc_bool_t btn_status;
    sc_misc_bt_t boot_type;

    sc_misc_get_button_status(imx8x_ipc(), &btn_status);

    /* Always stop when button is pressed */
    if (btn_status == 1) {
        return -PB_ERR_ABORT;
    }

    sc_misc_get_boot_type(imx8x_ipc(), &boot_type);

    /* Don't stop boot flow when command mode requested the boot */
    if (!(boot_get_flags() & BOOT_FLAG_CMD) && (boot_type == SC_MISC_BT_SERIAL)) {
        return -PB_ERR_ABORT;
    }

    return PB_OK;
}

static int board_status(uint8_t *response_bfr, size_t *response_size)
{
    uint32_t scu_version;
    uint32_t scu_commit;
    uint32_t seco_version;
    uint32_t seco_commit;
    int16_t celsius;
    int8_t tenths;

    char *response = (char *)response_bfr;
    size_t resp_buf_size = *response_size;

    sc_misc_build_info(imx8x_ipc(), &scu_version, &scu_commit);
    sc_seco_build_info(imx8x_ipc(), &seco_version, &seco_commit);
    sc_misc_get_temp(imx8x_ipc(), SC_R_SYSTEM, SC_MISC_TEMP, &celsius, &tenths);

    (*response_size) = snprintf(response,
                                resp_buf_size,
                                "SCFW:    %u, %x\n"
                                "SECO:    %u, %x\n"
                                "SOC ID:  %u\n"
                                "SOC REV: %u\n"
                                "CPU Temperature: %i.%i deg C",
                                scu_version,
                                scu_commit,
                                seco_version,
                                seco_commit,
                                imx8x_plat()->soc_id,
                                imx8x_plat()->soc_rev,
                                celsius,
                                tenths);

    response[(*response_size)++] = 0;

    return PB_OK;
}

static int board_password_auth(const char *password, size_t length)
{
    /* This is just a simplistic example of password authentication.
     *
     * A real implementation should use some kind of OTP storage for a
     * device unique password that's not easily accesible.
     * For example RPMB if there is an eMMC present.
     */
    LOG_DBG("Got password '%s', length = %zu", password, length);

    if (strncmp("imx8qxpmek", password, length) == 0 && length > 0)
        return PB_OK;
    else
        return -PB_ERR_AUTHENTICATION_FAILED;
}

static int board_set_slc_configuration(void)
{
    int rc;
    /**
     * Warning: This is an example configuration. Be carful with the slc
     * commands.
     *
     * Setting SLC Configuration will fuse SRK values matching the public keys
     * that can be found in the 'pki' directory.
     *
     * This will also configure the SoC to boot from eMMC0 with 8-bit DDR HS.
     */

    static const struct imx8x_srk_fuses srk_fuses = {
        .srk[0] = 0x6147e2e6,
        .srk[1] = 0xfc4dc849,
        .srk[2] = 0xb410b214,
        .srk[3] = 0x0f8d6212,
        .srk[4] = 0xad38b486,
        .srk[5] = 0x9b806149,
        .srk[6] = 0xdd6d397a,
        .srk[7] = 0x4c19d87b,
        .srk[8] = 0x24ac2acd,
        .srk[9] = 0xb6222a62,
        .srk[10] = 0xf36d6bd1,
        .srk[11] = 0x14cc8e16,
        .srk[12] = 0xd749170e,
        .srk[13] = 0x22fb187e,
        .srk[14] = 0x158f740c,
        .srk[15] = 0x8966b0f6,
    };

    rc = imx8x_fuse_write_srk(&srk_fuses);

    if (rc != PB_OK) {
        LOG_ERR("SRK fusing failed (%i)", rc);
        return rc;
    }

    rc = imx8x_fuse_write(IMX8X_FUSE_BOOT0, 0x00000002);

    if (rc != PB_OK) {
        LOG_ERR("BOOT0 fusing failed (%i)", rc);
        return rc;
    }

    rc = imx8x_fuse_write(IMX8X_FUSE_BOOT1, 0x00000025);

    if (rc != PB_OK) {
        LOG_ERR("BOOT1 fusing failed (%i)", rc);
        return rc;
    }

    return PB_OK;
}

void board_console_init(struct imx8x_platform *_plat)
{
    sc_pm_clock_rate_t rate = 80000000;
    uintptr_t uart_base = 0;

    uart_base = 0x5A060000;
    /* Power up UART0 */
    sc_pm_set_resource_power_mode(_plat->ipc, SC_R_UART_0, SC_PM_PW_MODE_ON);

    /* Set UART0 clock root to 80 MHz */
    sc_pm_set_clock_rate(_plat->ipc, SC_R_UART_0, SC_PM_CLK_PER, &rate);

    /* Enable UART0 clock root */
    sc_pm_clock_enable(_plat->ipc, SC_R_UART_0, SC_PM_CLK_PER, true, false);

    /* Configure UART pads */
    sc_pad_set(_plat->ipc, SC_P_UART0_RX, UART_PAD_CTRL);
    sc_pad_set(_plat->ipc, SC_P_UART0_TX, UART_PAD_CTRL);

    (void)imx_lpuart_init(uart_base, rate, 115200);

    static const struct console_ops ops = {
        .putc = imx_lpuart_putc,
    };

    console_init(uart_base, &ops);
}

static int board_hw_init(void)
{
    int rc;

    /* Initialize CAAM JR2, JR0 and JR1 are owned by the SECO */
    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);

    rc = imx_caam_init(IMX_CAAM_JR2_BASE);

    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    rc = usdhc_emmc_setup();

    if (rc != PB_OK)
        LOG_ERR("usdhc init failed (%i)", rc);

    return rc;
}

static int board_cm_init(void)
{
    int rc;

    /* Request power domains */
    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_USB_2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(imx8x_ipc(), SC_R_USB_2_PHY, SC_PM_PW_MODE_ON);

    /* Request clocks */
    rc = sc_pm_clock_enable(imx8x_ipc(), SC_R_USB_2, SC_PM_CLK_PER, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("USB_2 per clk enable failed!");
        return -PB_ERR;
    }

    rc = sc_pm_clock_enable(imx8x_ipc(), SC_R_USB_2, SC_PM_CLK_MISC, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("USB_2 misc clk enable failed!");
        return -PB_ERR;
    }

    rc = sc_pm_clock_enable(imx8x_ipc(), SC_R_USB_2, SC_PM_CLK_MST_BUS, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("USB_2 mst bus clk enable failed!");
        return -PB_ERR;
    }

    static const struct imx_cdns3_udc_config usb_cfg = {
        .base = 0x5B120000,
        .non_core_base = 0x5B110000,
        .phy_base = 0x5B160000,
    };

    return imx_cdns3_udc_init(&usb_cfg);
}

static const struct rot_config rot = {
    .revoke_key = imx8x_revoke_key,
    .read_key_status = imx8x_read_key_status,
};

static const struct slc_config slc = {
    .read_status = imx8x_slc_read_status,
    .set_configuration = board_set_slc_configuration,
    .set_configuration_locked = imx8x_slc_set_configuration_locked,
};

/*
 * TF-A BL31 at 0x80000000, then Linux as BL33 at EL2. BL31 takes BL33
 * from the bl_params list in x0 (testbench/board/imx8qxmek/patches);
 * an unpatched imx8qx BL31 jumps to 0x80020000, which is pb.
 */
static const struct boot_driver_atf_config boot_atf = {
    .atf_bpak_id = 0xa697d988, /* bpak_id("atf") */
    .bl33_bpak_id = 0xec103b08, /* bpak_id("kernel") */
    .dtb_bpak_id = 0x56f91b86, /* bpak_id("dt") */
    .ramdisk_bpak_id = 0xf4cdac1f, /* bpak_id("ramdisk") */
    .dtb_patch_cb = board_patch_bootargs,
    .resolve_part_name = boot_ab_part_uu_to_name,
};

const struct board board = {
    .name = "imx8qxpmek",
    .rot = &rot,
    .slc = &slc,
    .init = board_hw_init,
    .gpt_dev_uu = PART_user,
    .gpt_tables = board_layout_gpt,
    .gpt_tables_length = ARRAY_SIZE(board_layout_gpt),
    .ab = &board_layout_ab,
    .boot_atf = &boot_atf,
    .early_boot = early_boot,
    .bootargs = "console=ttyLP0,115200 quiet ",
    .bootargs_verbose = "console=ttyLP0,115200  "
                        "earlycon=adma_lpuart32,0x5a060000,115200 earlyprintk ",
    .cm_init = board_cm_init,
    .cm_transport = &pb_dev_cls_transport,
    .status = board_status,
    .password_auth = board_password_auth,
};
