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
#include <uuid.h>
#include <libfdt.h>
#include <pb/pb.h>
#include <pb/io.h>
#include <pb/plat.h>
#include <pb/fuse.h>
#include <pb/board.h>
#include <pb/delay.h>
#include <pb/timestamp.h>
#include <plat/imx8x/sci/svc/seco/sci_seco_api.h>
#include <plat/imx8x/imx8x.h>
#include <arch/armv8a/timer.h>
#include <arch/arch_helpers.h>
#include <drivers/mmc/mmc_core.h>
#include <drivers/partition/gpt.h>
#include <drivers/mmc/imx_usdhc.h>
#include <drivers/usb/usb_core.h>
#include <drivers/usb/imx_ehci.h>
#include <drivers/usb/imx_usb2_phy.h>
#include <drivers/crypto/imx_caam.h>
#include <boot/boot.h>
#include <boot/ab_state.h>
#include <boot/linux.h>
#include <crypto.h>

#include "partitions.h"
#define USDHC_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                         (SC_PAD_CONFIG_NORMAL << PADRING_CONFIG_SHIFT) | \
                         (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                         (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) | \
                         (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

#define USDHC_CLK_PAD_CTRL    (PADRING_IFMUX_EN_MASK | PADRING_GP_EN_MASK | \
                             (SC_PAD_CONFIG_OUT_IN << PADRING_CONFIG_SHIFT) | \
                             (SC_PAD_ISO_OFF << PADRING_LPCONFIG_SHIFT) | \
                             (SC_PAD_28FDSOI_DSE_18V_HS << PADRING_DSE_SHIFT) | \
                             (SC_PAD_28FDSOI_PS_PU << PADRING_PULL_SHIFT))

static struct imx8x_platform *plat;

struct fuse fuses[] =
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

static const struct gpt_part_table gpt_tbl[]=
{
    {
        .uu = UUID_2af755d8_8de5_45d5_a862_014cfa735ce0,
        .description = "System A",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_c046ccd8_0f2e_4036_984d_76c14dc73992,
        .description = "System B",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_c284387a_3377_4c0f_b5db_1bcbcff1ba1a,
        .description = "Root A",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_ac6a1b62_7bd0_460b_9e6a_9a7831ccbfbb,
        .description = "Root B",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_f5f8c9ae_efb5_4071_9ba9_d313b082281e,
        .description = "PB State Primary",
        .size = 512,
        .valid = true,
    },
    {
        .uu = UUID_656ab3fc_5856_4a5e_a2ae_5a018313b3ee,
        .description = "PB State Backup",
        .size = 512,
        .valid = true,
    },
    {
        .uu = UUID_4581af22_99e6_4a94_b821_b60c42d74758,
        .description = "Root overlay A",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_da2ca04f_a693_4284_b897_3906cfa1eb13,
        .description = "Root overlay B",
        .size = SZ_MB(30),
        .valid = true,
    },
    {
        .uu = UUID_23477731_7e33_403b_b836_899a0b1d55db,
        .description = "RoT extension A",
        .size = SZ_kB(128),
        .valid = true,
    },
    {
        .uu = UUID_6ffd077c_32df_49e7_b11e_845449bd8edd,
        .description = "RoT extension B",
        .size = SZ_kB(128),
        .valid = true,
    },
    {
        .uu = UUID_9697399d_e2da_47d9_8eb5_88daea46da1b,
        .description = "System storage A",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_c5b8b41c_0fb5_494d_8b0e_eba400e075fa,
        .description = "System storage B",
        .size = SZ_MB(128),
        .valid = true,
    },
    {
        .uu = UUID_39792364_d3e3_4013_ac51_caaea65e4334,
        .description = "Mass storage",
        .size = SZ_GB(1),
        .valid = true,
    },
    {
        .uu = NULL,
        .description = NULL,
        .size = 0,
        .valid = false,
    },
};

const uint32_t rom_key_map[] =
{
    0xa90f9680,
    0x25c6dd36,
    0x52c1eda0,
    0xcca57803,
    0x00000000,
};

static int patch_bootargs(void *fdt, int offset)
{
    const char *bootargs = NULL;

    if (boot_get_flags() & BOOT_FLAG_VERBOSE) {
        bootargs = "console=ttyLP0,115200  " \
                   "earlycon=adma_lpuart32,0x5a060000,115200 earlyprintk ";
    } else {
        bootargs = "console=ttyLP0,115200 quiet ";
    }

    int rc = fdt_setprop_string(fdt, offset, "bootargs", bootargs);

    if (rc != 0) {
        LOG_ERR("Board DTB patch failed (%i)", rc);
        return -PB_ERR;
    }
    return PB_OK;
}

static int usdhc_emmc_setup(void)
{
    int rc;
    unsigned int rate;

    sc_pm_set_resource_power_mode(plat->ipc, SC_R_SDHC_0, SC_PM_PW_MODE_ON);
    sc_pm_clock_enable(plat->ipc, SC_R_SDHC_0, SC_PM_CLK_PER, false, false);

    rc = sc_pm_set_clock_parent(plat->ipc, SC_R_SDHC_0, 2, SC_PM_PARENT_PLL1);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("usdhc set clock parent failed");
        return -PB_ERR;
    }

    rate = MHz(200);
    sc_pm_set_clock_rate(plat->ipc, SC_R_SDHC_0, 2, &rate);

    rc = sc_pm_clock_enable(plat->ipc, SC_R_SDHC_0, SC_PM_CLK_PER, true, false);

    if (rc != SC_ERR_NONE) {
        LOG_ERR("SDHC_0 per clk enable failed!");
        return -PB_ERR;
    }

    sc_pad_set(plat->ipc, SC_P_EMMC0_CLK, USDHC_CLK_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_CMD, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA0, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA1, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA2, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA3, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA4, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA5, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA6, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_DATA7, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_STROBE, USDHC_PAD_CTRL);
    sc_pad_set(plat->ipc, SC_P_EMMC0_RESET_B, USDHC_PAD_CTRL);

    static const struct imx_usdhc_config cfg = {
        .base = 0x5B010000,
        .delay_tap = 18,
        .mmc_config = {
            .mode = MMC_BUS_MODE_HS200,
            .width = MMC_BUS_WIDTH_8BIT,
            .boot_mode = EXT_CSD_BOOT_DDR | EXT_CSD_BOOT_BUS_WIDTH_8,
            .boot0_uu = PART_boot0,
            .boot1_uu = PART_boot1,
            .user_uu = PART_user,
            .rpmb_uu = PART_rpmb,
            .flags = 0,
        }
    };

    return imx_usdhc_init(&cfg, rate);
}

static int early_boot(void)
{
    sc_bool_t btn_status;
    sc_misc_bt_t boot_type;

    sc_misc_get_button_status(plat->ipc, &btn_status);

    /* Always stop when button is pressed */
    if (btn_status == 1) {
        return -PB_ERR_ABORT;
    }

    sc_misc_get_boot_type(plat->ipc, &boot_type);

    /* Don't stop boot flow when command mode requested the boot */
    if (!(boot_get_flags() & BOOT_FLAG_CMD) && (boot_type == SC_MISC_BT_SERIAL)) {
        return -PB_ERR_ABORT;
    }

    return PB_OK;
}

const char * board_name(void)
{
    return "imx8qxmek";
}

int board_command(void *plat_ptr,
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

int board_status(void *plat_ptr,
                    void *response_bfr,
                    size_t *response_size)
{
    uint32_t scu_version;
    uint32_t scu_commit;
    uint32_t seco_version;
    uint32_t seco_commit;
    int16_t celsius;
    int8_t tenths;

    char *response = (char *) response_bfr;
    size_t resp_buf_size = *response_size;

    sc_misc_build_info(plat->ipc, &scu_version, &scu_commit);
    sc_seco_build_info(plat->ipc, &seco_version, &seco_commit);
    sc_misc_get_temp(plat->ipc, SC_R_SYSTEM, SC_MISC_TEMP, &celsius,
                        &tenths);

    (*response_size) = snprintf(response, resp_buf_size,
                            "SCFW:    %u, %x\n" \
                            "SECO:    %u, %x\n" \
                            "SOC ID:  %u\n" \
                            "SOC REV: %u\n" \
                            "CPU Temperature: %i.%i deg C",
                            scu_version, scu_commit,
                            seco_version, seco_commit,
                            plat->soc_id, plat->soc_rev,
                            celsius, tenths);

    response[(*response_size)++] = 0;

    return PB_OK;
}

#ifdef CONFIG_AUTH_PASSWORD
int board_command_mode_auth(char *password, size_t length)
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
#endif  // CONFIG_AUTH_PASSWORD

int board_init(struct imx8x_platform *plat_ptr)
{
    int rc;
    plat = plat_ptr;

    /* Initialize CAAM JR2, JR0 and JR1 are owned by the SECO */
    ts("CAAM init start");
    sc_pm_set_resource_power_mode(plat->ipc,
                                SC_R_CAAM_JR2, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(plat->ipc,
                                SC_R_CAAM_JR2_OUT, SC_PM_PW_MODE_ON);

    rc = imx_caam_init(0x31430000);
    ts("CAAM init end");
    if (rc != PB_OK) {
        LOG_ERR("CAAM init failed (%i)", rc);
        return rc;
    }

    /* TODO: Rework and move the 'usdhc_emmc_setup' to imx8x platform,
     * And make it optional through KConfig. This way the upstream platform
     * code will cover most use cases and the special ones can still quite
     * easily be imlemented on board level.
     */
    ts("usdhc start");
    rc = usdhc_emmc_setup();

    ts("usdhc end");
    if (rc != PB_OK) {
        LOG_ERR("usdhc init failed (%i)", rc);
        goto err_out;
    }

    bio_dev_t user_part = bio_get_part_by_uu(PART_user);

    if (user_part < 0) {
        goto err_out;
    }

    ts("gpt start");
    rc = gpt_ptbl_init(user_part, gpt_tbl);
    /* eMMC User partition now only has the visible flag to report capacity */
    (void) bio_set_flags(user_part, BIO_FLAG_VISIBLE);
    ts("gpt end");

    if (rc != PB_OK) {
        LOG_ERR("GPT ptbl init failed (%i)", rc);
        goto err_out;
    }

    static const struct boot_ab_state_config boot_state_cfg = {
        .primary_state_part_uu = PART_primary_state,
        .backup_state_part_uu = PART_backup_state,
        .sys_a_uu = PART_sys_a,
        .sys_b_uu = PART_sys_b,
        .rollback_mode = AB_ROLLBACK_MODE_NORMAL,
    };

    rc = boot_ab_state_init(&boot_state_cfg);

    if (rc != PB_OK) {
        goto err_out;
    }

    static const struct boot_driver_linux_config linux_boot_cfg = {
        .image_bpak_id     = 0xa697d988,    /* bpak_id("atf") */
        .dtb_bpak_id       = 0x56f91b86,    /* bpak_id("dt")  */
        .ramdisk_bpak_id   = 0xf4cdac1f,    /* bpak_id("ramdisk") */
        .dtb_patch_cb      = patch_bootargs,
        .resolve_part_name = boot_ab_part_uu_to_name,
        .set_dtb_boot_arg  = false,
    };

    rc = boot_driver_linux_init(&linux_boot_cfg);

    if (rc != PB_OK) {
        goto err_out;
    }

    static const struct boot_driver boot_driver = {
        .default_boot_source = BOOT_SOURCE_BIO,
        .early_boot_cb       = early_boot,
        .get_boot_bio_device = boot_ab_state_get,
        .set_boot_partition  = boot_ab_state_set_boot_partition,
        .get_boot_partition  = boot_ab_state_get_boot_partition,
        .prepare             = boot_driver_linux_prepare,
        .late_boot_cb        = NULL,
        .jump                = boot_driver_linux_jump,
    };

    rc = boot_init(&boot_driver);

    if (rc != PB_OK) {
        goto err_out;
    }

    /* Enable usb stuff */
    sc_pm_set_resource_power_mode(plat->ipc, SC_R_USB_0, SC_PM_PW_MODE_ON);
    sc_pm_set_resource_power_mode(plat->ipc, SC_R_USB_0_PHY, SC_PM_PW_MODE_ON);

    imx_usb2_phy_init(0x5B100000);

err_out:
    return rc;
}
